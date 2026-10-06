/*
 * PS5 autotest, installed by the launcher in autotest builds, which prepends
 * a PS5_AUTOTEST object:
 *   var PS5_AUTOTEST = { mode: "tour" | "play",
 *                        scenarios: ["/app0/assets/rct2/Scenarios/....SC6", ...],
 *                        secondsPerPark: 20,
 *                        play: "/app0/assets/rct2/Scenarios/....SC6" };
 *
 * Results go to the console, which the PS5 build echoes to the kernel log
 * (read it with klogsrv on port 3232).
 *
 * tour: on the title screen, load every scenario in turn, run it at the
 *   fastest speed and log one line per scenario:
 *     autotest: [12/57] Crazy Castle: ok, 7.5x real time (target 8x), 312 guests, rating 650
 *   The tour repeats, logging a summary after each pass.
 *
 * play: the launcher starts a scenario (the next one on each launch). The test
 *   opens the park, takes a loan, hires a handyman, starts marketing, funds research, extends a
 *   footpath and builds a food or drink stall next to it, then fast-forwards a
 *   year while logging the park each month, and ends with a summary:
 *     autotest: play PASS Crazy Castle: 7/7 steps, guests 12 -> 460, ...
 */

var SPEED = 4; /* the title sequence's fastest speed */
var TARGET = Math.pow(2, SPEED - 1); /* speed n runs 2^(n-1) times real time */
var TICKS_PER_SECOND = 40; /* game ticks per second at normal speed */

function log(message) {
    console.log("autotest: " + message);
}

function baseName(path) {
    return path.substring(path.lastIndexOf("/") + 1);
}

function findSequence(name) {
    var sequences = titleSequenceManager.titleSequences;
    for (var i = 0; i < sequences.length; i++) {
        if (sequences[i].name === name) {
            return sequences[i];
        }
    }
    return null;
}

/* Build (or rebuild) the tour sequence from the installed scenarios. */
function buildSequence(config) {
    var name = "PS5 Autotest";
    var old = findSequence(name);
    if (old !== null && !old.isReadOnly) {
        old.delete();
    }

    var sequence = titleSequenceManager.create(name);
    var commands = [];
    for (var i = 0; i < config.scenarios.length; i++) {
        sequence.addPark(config.scenarios[i], baseName(config.scenarios[i]));
        commands.push({ type: "load", index: i });
        commands.push({ type: "speed", speed: SPEED });
        commands.push({ type: "wait", duration: config.secondsPerPark * 1000 });
    }
    commands.push({ type: "restart" });
    sequence.commands = commands;
    return sequence;
}

function runTour(config) {
    if (context.mode !== "title") {
        return;
    }
    var total = config.scenarios.length;
    log("starting tour of " + total + " scenarios, " + config.secondsPerPark + " s each");

    var current = null; /* the park being measured */
    var loaded = 0;
    var pass = 1;
    var passStart = Date.now();
    var slowest = null;

    function finishCurrent() {
        if (current === null) {
            return;
        }
        var seconds = (Date.now() - current.start) / 1000;
        var ratio = seconds > 0 ? current.ticks / (seconds * TICKS_PER_SECOND) : 0;
        log("[" + current.index + "/" + total + "] " + current.name + ": ok, " +
            ratio.toFixed(1) + "x real time (target " + TARGET + "x), " +
            park.guests + " guests, rating " + park.rating + ", cash " + park.cash / 10);
        if (slowest === null || ratio < slowest.ratio) {
            slowest = { name: current.name, ratio: ratio };
        }
        current = null;
    }

    context.subscribe("interval.tick", function () {
        if (current !== null) {
            current.ticks++;
        }
    });

    context.subscribe("map.changed", function () {
        if (context.mode !== "title") {
            return; /* the player started a game; stay quiet */
        }
        finishCurrent();
        loaded++;
        var index = ((loaded - 1) % total) + 1;
        if (index === 1 && loaded > 1) {
            log("pass " + pass + " complete: " + total + " scenarios in " +
                Math.round((Date.now() - passStart) / 1000) + " s, slowest " +
                (slowest ? slowest.name + " at " + slowest.ratio.toFixed(1) + "x" : "-"));
            pass++;
            passStart = Date.now();
            slowest = null;
        }
        current = { index: index, name: park.name, start: Date.now(), ticks: 0 };
    });

    try {
        buildSequence(config).play();
    } catch (e) {
        log("could not start the tour: " + e);
    }
}

/* ---- play test ------------------------------------------------------- */

var RIDE_TYPE_FOOD_STALL = 28;
var RIDE_TYPE_DRINK_STALL = 30;
var RIDE_TYPE_SHOP = 32;
var TRACK_FLAT_1X1A = 262; /* the one-tile piece every stall uses */
var RIDE_STATUS_OPEN = 1;
var PARK_PARAMETER_OPEN = 1;
var STAFF_HANDYMAN = 0;
var HANDYMAN_ALL_ORDERS = 15; /* sweep, water gardens, empty bins, mow */
var MARKETING_PARK = 4; /* advertising campaign for the whole park */
var RESEARCH_ALL_CATEGORIES = 0x7f;
var RESEARCH_MAXIMUM = 3;
var INVALID_DIRECTION = 0xff;
var TEST_MONTHS = 8; /* one in-game year (March to October) */

/* Tile offsets for directions 0-3 (west, north, east, south). */
var DX = [-1, 0, 1, 0];
var DY = [0, 1, 0, -1];

function money(value) {
    return (value / 10).toFixed(0);
}

function surfaceOf(tile) {
    for (var i = 0; i < tile.numElements; i++) {
        if (tile.elements[i].type === "surface") {
            return tile.elements[i];
        }
    }
    return null;
}

/* An owned, flat, dry tile holding nothing but land at height z. */
function isFreeLand(x, y, z) {
    if (x < 1 || y < 1 || x >= map.size.x - 1 || y >= map.size.y - 1) {
        return false;
    }
    var tile = map.getTile(x, y);
    if (tile.numElements !== 1) {
        return false;
    }
    var surface = tile.elements[0];
    return surface.type === "surface" && surface.hasOwnership && surface.slope === 0 &&
        surface.baseZ === z && surface.waterHeight <= surface.baseZ;
}

var ENTRANCE_TYPE_PARK = 2;

function footpathAt(x, y, z) {
    if (x < 0 || y < 0 || x >= map.size.x || y >= map.size.y) {
        return null;
    }
    var tile = map.getTile(x, y);
    for (var i = 0; i < tile.numElements; i++) {
        var el = tile.elements[i];
        /* Sloped paths meet their neighbours up to one step (16) higher or lower. */
        if (el.type === "footpath" && !el.isQueue && Math.abs(el.baseZ - z) <= 16) {
            return el;
        }
    }
    return null;
}

function parkEntrancePaths() {
    var starts = [];
    for (var y = 0; y < map.size.y; y++) {
        for (var x = 0; x < map.size.x; x++) {
            var tile = map.getTile(x, y);
            for (var i = 0; i < tile.numElements; i++) {
                var el = tile.elements[i];
                if (el.type !== "entrance" || el.object !== ENTRANCE_TYPE_PARK) {
                    continue;
                }
                for (var d = 0; d < 4; d++) {
                    var path = footpathAt(x + DX[d], y + DY[d], el.baseZ);
                    if (path !== null) {
                        starts.push({ x: x + DX[d], y: y + DY[d], el: path });
                    }
                }
            }
        }
    }
    return starts;
}

/*
 * Walk the footpaths guests can reach, outward from the park entrance, and
 * return the first flat path on owned land with free land on (at least) two
 * sides: one for the stall and one to extend the path into.
 */
function findSite() {
    var queue = parkEntrancePaths();
    var seen = {};
    while (queue.length > 0) {
        var cur = queue.shift();
        var key = cur.x + "," + cur.y + "," + cur.el.baseZ;
        if (seen[key]) {
            continue;
        }
        seen[key] = true;

        var surface = surfaceOf(map.getTile(cur.x, cur.y));
        if (cur.el.slopeDirection === null && surface !== null && surface.hasOwnership) {
            var free = [];
            for (var d = 0; d < 4; d++) {
                if (isFreeLand(cur.x + DX[d], cur.y + DY[d], cur.el.baseZ)) {
                    free.push(d);
                }
            }
            if (free.length >= 2) {
                return { x: cur.x, y: cur.y, z: cur.el.baseZ, path: cur.el, free: free };
            }
        }

        for (var e = 0; e < 4; e++) {
            if (cur.el.edges & (1 << e)) {
                var next = footpathAt(cur.x + DX[e], cur.y + DY[e], cur.el.baseZ);
                if (next !== null) {
                    queue.push({ x: cur.x + DX[e], y: cur.y + DY[e], el: next });
                }
            }
        }
    }
    return null;
}

/* A stall to build: food sells fastest, then drinks, then anything else. */
function stallObject() {
    var rides = objectManager.getAllObjects("ride");
    var preference = [RIDE_TYPE_FOOD_STALL, RIDE_TYPE_DRINK_STALL, RIDE_TYPE_SHOP];
    for (var p = 0; p < preference.length; p++) {
        for (var i = 0; i < rides.length; i++) {
            if (rides[i].rideType[0] === preference[p]) {
                return rides[i];
            }
        }
    }
    return null;
}

function pathEdges(x, y, z) {
    var tile = map.getTile(x, y);
    for (var i = 0; i < tile.numElements; i++) {
        var el = tile.elements[i];
        if (el.type === "footpath" && el.baseZ === z) {
            return el.edges;
        }
    }
    return 0;
}

function runPlay(config) {
    var steps = [];
    var start = null;
    var stallRide = null;
    var monthsDone = 0;
    var lastMonth = -1;
    var ticks = 0;
    var tickStart = 0;
    var started = false;

    function step(name, ok, detail) {
        steps.push({ name: name, ok: ok });
        log("play step " + (ok ? "ok" : "FAILED") + ": " + name + (detail ? " (" + detail + ")" : ""));
    }

    function skip(name, detail) {
        log("play step skipped: " + name + " (refused by the scenario: " + detail + ")");
    }

    /*
     * Run a game action, then call next(result) once it has executed. Optional
     * actions may be refused by a scenario's rules (no marketing, a fixed loan,
     * ...); that is logged as skipped rather than failed.
     */
    function act(name, action, args, next, optional) {
        context.executeAction(action, args, function (result) {
            var ok = !result.error;
            var detail = ok ? "" : result.errorTitle + ": " + result.errorMessage;
            if (!ok && optional) {
                skip(name, detail);
            } else {
                step(name, ok, detail);
            }
            if (next) {
                next(result);
            }
        });
    }

    function snapshot() {
        return {
            cash: park.cash, guests: park.guests, rating: park.rating,
            staff: map.getAllEntities("staff").length, rides: map.numRides
        };
    }

    /*
     * Place a stall beside the path, trying each free side and rotation (facing
     * the path first) until the path connects to it.
     */
    function buildStall(site, object, candidates, done) {
        if (candidates.length === 0) {
            step("build a stall the path connects to", false, "no placement connected");
            done();
            return;
        }
        var d = candidates[0].side;
        var rotation = candidates[0].rotation;
        var sx = site.x + DX[d], sy = site.y + DY[d];
        context.executeAction("ridecreate", {
            rideType: object.rideType[0], rideObject: object.index, entranceObject: 0,
            colour1: 0, colour2: 0, inspectionInterval: 0
        }, function (created) {
            if (created.error) {
                step("create stall ride", false, created.errorTitle + ": " + created.errorMessage);
                done();
                return;
            }
            var ride = created.ride;
            context.executeAction("trackplace", {
                x: sx * 32, y: sy * 32, z: site.z, direction: rotation, ride: ride,
                trackType: TRACK_FLAT_1X1A, rideType: object.rideType[0], brakeSpeed: 0,
                colour: 0, seatRotation: 4, trackPlaceFlags: 0, isFromTrackDesign: false
            }, function (placed) {
                if (!placed.error && (pathEdges(site.x, site.y, site.z) & (1 << d))) {
                    stallRide = ride;
                    step("build " + object.name + " at " + sx + "," + sy, true);
                    act("open the stall", "ridesetstatus", { ride: ride, status: RIDE_STATUS_OPEN }, function () {
                        done();
                    });
                    return;
                }
                /* It did not fit or does not face the path: remove it and try the next. */
                context.executeAction("ridedemolish", { ride: ride, modifyType: 0 }, function () {
                    buildStall(site, object, candidates.slice(1), done);
                });
            });
        });
    }

    function construct(done) {
        var site = findSite();
        var object = stallObject();
        if (site === null || object === null) {
            step("find a site and a stall", false, site === null ? "no free land beside a path" : "no stall object");
            done();
            return;
        }
        /* Extend the path into the last free side, keep the others for the stall. */
        var pd = site.free[site.free.length - 1];
        act("extend a footpath", "footpathplace", {
            x: (site.x + DX[pd]) * 32, y: (site.y + DY[pd]) * 32, z: site.z,
            direction: INVALID_DIRECTION, object: site.path.surfaceObject,
            railingsObject: site.path.railingsObject, slopeType: 0, slopeDirection: 0,
            constructFlags: 0
        }, function () {
            var candidates = [];
            site.free.slice(0, site.free.length - 1).forEach(function (side) {
                var towardPath = (side + 2) % 4;
                for (var r = 0; r < 4; r++) {
                    candidates.push({ side: side, rotation: (towardPath + r) % 4 });
                }
            });
            buildStall(site, object, candidates, done);
        });
    }

    function finish() {
        var seconds = (Date.now() - tickStart) / 1000;
        var ratio = seconds > 0 ? ticks / (seconds * TICKS_PER_SECOND) : 0;
        var end = snapshot();
        var passed = steps.filter(function (s) { return s.ok; }).length;
        var stall = stallRide !== null ? map.getRide(stallRide) : null;
        log("play " + (passed === steps.length ? "PASS" : "PARTIAL") + " " + park.name + ": " +
            passed + "/" + steps.length + " steps, " + TEST_MONTHS + " months at " +
            ratio.toFixed(1) + "x real time, guests " + start.guests + " -> " + end.guests +
            ", rating " + start.rating + " -> " + end.rating + ", cash " + money(start.cash) +
            " -> " + money(end.cash) + ", staff " + start.staff + " -> " + end.staff +
            (stall ? ", stall customers " + stall.totalCustomers + ", profit " + money(stall.totalProfit) : ""));
    }

    function onDay() {
        if (date.month === lastMonth || monthsDone >= TEST_MONTHS) {
            return;
        }
        lastMonth = date.month;
        monthsDone++;
        var stall = stallRide !== null ? map.getRide(stallRide) : null;
        log("play month " + monthsDone + "/" + TEST_MONTHS + ": guests " + park.guests +
            ", rating " + park.rating + ", cash " + money(park.cash) +
            (stall ? ", stall customers " + stall.totalCustomers : ""));
        if (monthsDone === TEST_MONTHS) {
            finish();
        }
    }

    function begin() {
        if (started || context.mode !== "normal") {
            return;
        }
        started = true;
        start = snapshot();
        log("play start " + park.name + ": guests " + start.guests + ", rating " + start.rating +
            ", cash " + money(start.cash) + ", loan " + money(park.bankLoan) + "/" +
            money(park.maxBankLoan) + ", rides " + start.rides +
            ", park " + (park.getFlag("open") ? "open" : "closed"));

        /* Opening moves, in order. Scenario rules may refuse the optional ones. */
        var moves = [
            { name: "open the park", action: "parksetparameter",
              args: { parameter: PARK_PARAMETER_OPEN, value: 0 } },
            { name: "take the maximum loan", action: "parksetloan",
              args: { value: park.maxBankLoan }, optional: true },
            { name: "hire a handyman", action: "staffhire",
              args: { autoPosition: true, staffType: STAFF_HANDYMAN, costumeIndex: 0,
                      staffOrders: HANDYMAN_ALL_ORDERS } },
            { name: "start a park marketing campaign", action: "parkmarketing",
              args: { type: MARKETING_PARK, item: 0, duration: 4 }, optional: true },
            { name: "fund research", action: "parksetresearchfunding",
              args: { priorities: RESEARCH_ALL_CATEGORIES, fundingAmount: RESEARCH_MAXIMUM }, optional: true }
        ];

        function fastForward() {
            act("fast-forward", "gamesetspeed", { speed: SPEED }, function () {
                lastMonth = date.month;
                tickStart = Date.now();
                context.subscribe("interval.tick", function () { ticks++; });
                context.subscribe("interval.day", onDay);
            });
        }

        function nextMove(i) {
            if (i === moves.length) {
                construct(fastForward);
                return;
            }
            var m = moves[i];
            act(m.name, m.action, m.args, function () { nextMove(i + 1); }, m.optional);
        }
        nextMove(0);
    }

    context.subscribe("map.changed", begin);
    begin();
}

function main() {
    if (typeof PS5_AUTOTEST === "undefined") {
        return;
    }
    if (PS5_AUTOTEST.mode === "play") {
        runPlay(PS5_AUTOTEST);
    } else {
        runTour(PS5_AUTOTEST);
    }
}

registerPlugin({
    name: "ps5-autotest",
    version: "1.0",
    authors: ["rct2-ps5"],
    type: "intransient",
    licence: "GPL-3.0-or-later",
    targetApiVersion: 122,
    main: main
});
