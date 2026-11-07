# Achievement service debt — chess-fix-gamekit

`-[GKAchievement reportAchievementWithCompletionHandler:]`,
`+[GKAchievement reportAchievements:withCompletionHandler:]` and
`+[GKAchievement resetAchievementsWithCompletionHandler:]` in `src/GameKit.m`
cannot report/reset server progress: Darling has no authenticated Game Center
service, app entitlement integration or achievement store. Chess needs reporting
to complete its move handling. These methods fail asynchronously on the main
queue without changing local progress or pretending to update server state.

Retained policy (GUESS): return `GKErrorDomain/GKErrorNotSupported` (16) because
this runtime has Game Center support disabled. Apple's documented notSupported
means the app lacks Game Center enablement; it does not mean a player who has
not signed in. This policy is not inferred from the macOS no-account observation.
Replace these failures with service-backed reporting/reset when that service is
available; local persistence alone cannot implement Game Center's contract.

Rung 3: https://developer.apple.com/tutorials/data/documentation/gamekit/gkerror/code/notsupported.json
and https://developer.apple.com/tutorials/data/documentation/gamekit/gkerror/code/notauthenticated.json
