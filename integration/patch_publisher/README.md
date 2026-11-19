# Windows publisher timeout recovery

The fixed two-hour command deadline interrupted an active Windows publication.
The Editor now watches output and actual transfer/hash progress, allows cancellation,
and stops the owned process tree after 15 minutes without activity. Diagnostic tails
are bounded. There is no total-duration deadline while work continues.

The FTPS publishers report advancing upload and download-verification progress.
Each successfully verified immutable client blob is checkpointed atomically
in the destination-bound receipt. Interrupted releases can resume verified blobs.
Existing signing, prepared-release validation and metadata activation order remain.

Source files mirror their paths under the Unity project root. No credentials or
local publisher configuration are included. Run Tools/Patching/Test-PublisherProgress.ps1
in Windows PowerShell: watchdog simulation exceeding two hours, silence timeout,
late callbacks, streamed SHA256, stationary transfer events, and partial upload resume.

Validation: PUBLISHER_PROGRESS_TESTS=PASS (Windows PowerShell 5).

## Missing runtime address regression

The published 1.0.10 catalog omitted 7,137 of 15,300 explicit runtime addresses, including the starting map and effects. Package checksums and required-label downloads alone did not detect this. Configuration now preserves address/GUID/label keys for public remote roots. Manifest generation validates every explicit non-shared runtime address against the serialized catalog before preparing or publishing a release. The recovery first reproduces rejection of 1.0.10, then creates a fresh baseline and matching signed player.
