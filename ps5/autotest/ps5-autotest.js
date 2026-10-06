/*
 * PS5 autotest: tours every installed scenario on the title screen.
 *
 * Installed by the launcher in autotest builds (OPENRCT2_PS5_AUTOTEST=1),
 * which prepends a PS5_AUTOTEST object:
 *   var PS5_AUTOTEST = { scenarios: ["/app0/assets/rct2/Scenarios/....SC6", ...],
 *                        secondsPerPark: 20 };
 *
 * It builds a title sequence that loads each scenario, runs it at fast speed
 * and moves on, and logs one line per scenario to the console (echoed to the
 * PS5 kernel log; read it with klogsrv on port 3232):
 *   autotest: [12/57] Crazy Castle: ok, 2.4x real time, 312 guests, rating 650
 * The tour repeats, logging a summary after each pass.
 */

var SPEED = 4; /* the title sequence's fastest speed */
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

function main() {
    if (typeof PS5_AUTOTEST === "undefined" || context.mode !== "title") {
        return;
    }
    var config = PS5_AUTOTEST;
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
            ratio.toFixed(1) + "x real time (target " + SPEED + "x), " +
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

registerPlugin({
    name: "ps5-autotest",
    version: "1.0",
    authors: ["rct2-ps5"],
    type: "intransient",
    licence: "GPL-3.0-or-later",
    targetApiVersion: 122,
    main: main
});
