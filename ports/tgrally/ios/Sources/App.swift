// App.swift: Top Gear Rally on the iPhone. Landscape only; the game is drawn
// into a 16:9 Metal layer centred on the screen (the Remastered profile's race
// fills it, menus keep the N64's 4:3 inside it), with the touch surface over
// the whole screen. The game runs its own main loop (os/main.c) on a thread
// of its own once the layer is laid out.

import Metal
import QuartzCore
import UIKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    func application(_ application: UIApplication,
                     configurationForConnecting session: UISceneSession,
                     options: UIScene.ConnectionOptions) -> UISceneConfiguration {
        let c = UISceneConfiguration(name: nil, sessionRole: session.role)
        c.delegateClass = SceneDelegate.self
        return c
    }
}

final class SceneDelegate: UIResponder, UIWindowSceneDelegate {
    var window: UIWindow?

    func scene(_ scene: UIScene, willConnectTo session: UISceneSession,
               options connectionOptions: UIScene.ConnectionOptions) {
        guard let ws = scene as? UIWindowScene else { return }
        let w = UIWindow(windowScene: ws)
        w.rootViewController = GameViewController()
        w.makeKeyAndVisible()
        window = w
    }

    // in the background the game is paused (the N64's clock stops) with nothing held
    func sceneWillResignActive(_ scene: UIScene) {
        (window?.rootViewController as? GameViewController)?.surface.reset()
        Events.focus(false)
    }

    func sceneDidBecomeActive(_ scene: UIScene) { Events.focus(true) }
}

final class MetalView: UIView {
    override class var layerClass: AnyClass { CAMetalLayer.self }
}

final class GameViewController: UIViewController {
    let screenView = MetalView()
    let surface = TouchSurface()
    private var started = false

    override func loadView() {
        let root = UIView()
        root.backgroundColor = .black
        screenView.isUserInteractionEnabled = false
        screenView.backgroundColor = .black
        root.addSubview(screenView)
        root.addSubview(surface)
        view = root
        let ml = screenView.layer as! CAMetalLayer
        ml.pixelFormat = .bgra8Unorm
        ml.framebufferOnly = true
        ml.device = MTLCreateSystemDefaultDevice()
        Screen.layer = ml
        AudioFilter.apply()
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        let b = view.bounds
        surface.frame = b
        // the largest 16:9 the screen holds, centred
        var w = b.width, h = (w * 9 / 16).rounded()
        if h > b.height { h = b.height; w = (h * 16 / 9).rounded() }
        screenView.frame = CGRect(x: ((b.width - w) / 2).rounded(), y: ((b.height - h) / 2).rounded(), width: w, height: h)
        surface.gameFrame = screenView.frame
        let scale = view.window?.screen.nativeScale ?? UIScreen.main.nativeScale
        let ml = screenView.layer as! CAMetalLayer
        ml.contentsScale = scale
        let px = CGSize(width: (w * scale).rounded(), height: (h * scale).rounded())
        ml.drawableSize = px
        Screen.setPixels(Int32(px.width), Int32(px.height))
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        guard !started, Screen.pixels().0 > 0 else { return }
        started = true
        tgr_race_engine(BossRally.raceEngine)
        if ProcessInfo.processInfo.environment["BR_HANDOFF_TEST"] != nil {
            // a check of Boss Rally alone: its test race (race_handoff.c), in front from the start
            Engines.bringToFront(.br)
            BossRally.start()
            return
        }
        let t = Thread {
            Engines.tgrThread = pthread_self()
            var argv: [UnsafeMutablePointer<CChar>?] = [strdup("tgrally"), nil]
            _ = tgr_main(1, &argv)
        }
        t.name = "tgrally"
        t.stackSize = 16 << 20
        t.qualityOfService = .userInteractive
        t.start()
    }

    override var prefersStatusBarHidden: Bool { true }
    override var prefersHomeIndicatorAutoHidden: Bool { true }
    override var preferredScreenEdgesDeferringSystemGestures: UIRectEdge { .all }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .landscape }
}
