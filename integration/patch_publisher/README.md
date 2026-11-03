# Windows publisher timeout recovery

The fixed two-hour command deadline interrupted an active Windows publication.
The Editor now watches output and actual transfer/hash progress, allows cancellation,
and stops the owned process tree after 15 minutes without activity. Diagnostic tails
are bounded. There is no total-duration deadline while work continues.

The FTPS publishers report advancing upload and download-verification progress.
Each successfully HTTPS-hash-verified immutable client blob is checkpointed atomically
in the destination-bound receipt. Interrupted releases can resume verified blobs.
Existing signing, prepared-release validation and metadata activation order remain.

Source files mirror their paths under the Unity project root. No credentials or
local publisher configuration are included. Run Tools/Patching/Test-PublisherProgress.ps1
in Windows PowerShell: watchdog simulation exceeding two hours, silence timeout,
late callbacks, streamed SHA256, stationary transfer events, and partial upload resume.

Validation: PUBLISHER_PROGRESS_TESTS=PASS (Windows PowerShell 5).
