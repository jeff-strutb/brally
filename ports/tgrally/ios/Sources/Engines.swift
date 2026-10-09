// Engines.swift: the two games in the one app. Top Gear Rally runs from
// launch and owns the menus; with its Remastered flag on (TGR_FLAG_BR_RACES)
// each race the player starts is handed to Boss Rally's engine
// (host_race.h), which boots on the first one and then waits at its own
// menus between races. One game is in front at a time: the host gives it the
// events, the sound and the screen (Host.swift), and touch drives it
// (TouchPad.swift).

import Foundation

enum Engine: Int { case tgr = 0, br = 1 }

enum Engines {
    private static let lock = NSLock()
    private static var front = Engine.tgr
    /// Top Gear Rally's host loop (os/main.c), the one thread of its that
    /// asks the host for events and opens the sound
    static var tgrThread: pthread_t?

    static var active: Engine {
        lock.lock(); defer { lock.unlock() }
        return front
    }

    /// Which game is asking the host
    static func caller() -> Engine {
        if let t = tgrThread, pthread_equal(pthread_self(), t) != 0 { return .tgr }
        return .br
    }

    static func bringToFront(_ e: Engine) {
        lock.lock(); front = e; lock.unlock()
        DispatchQueue.main.async { TouchSurface.current?.reset() }
    }
}

enum BossRally {
    private static var started = false

    /// Boss Rally's process on a thread of its own, told first that races
    /// will be asked of it (it waits at its menus for them)
    static func start() {
        guard !started else { return }
        started = true
        br_race_serve()
        let t = Thread {
            var argv: [UnsafeMutablePointer<CChar>?] = [strdup("brally"), nil]
            _ = br_main(1, &argv)
        }
        t.name = "brally"
        t.stackSize = 16 << 20
        t.qualityOfService = .userInteractive
        t.start()
    }

    /// Top Gear Rally's race engine (tgr_race_engine): called on its game
    /// thread, which waits here for the race's outcome
    static let raceEngine: host_race_fn = { race, res in
        guard let race, let res else { return 0 }
        BossRally.start()
        Engines.bringToFront(.br)
        let r = br_race_run(race, res)
        Engines.bringToFront(.tgr)
        return r
    }
}
