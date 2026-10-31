# CoreServices user activity forwarding

Unmodified official Discord0.0.420 and VSCode1.144.0 request the public
NSUserActivity class from CoreServices. The actual Foundation class already
exists; CoreServices lacks the two-level export. Forward its existing class and
metaclass through the current narrow reexport mechanism, with a direct provider
dependency. No duplicate class, empty implementation or whole-Foundation
reexport is added. Both symbol lists participate in incremental link dependencies.

The Foundation i386 build uses the legacy Objective-C class symbol ABI. Apply
the modern class exports only to x86_64 and the existing ARM64 configuration;
retain the current common i386 exports. Own fat x86_64/i386 library relinking
passes; ARM64 is configured but untested. This does not qualify Dia or ARM64.

An original executable explicitly links the umbrella for its class reference,
without directly linking Foundation. Its unchanged parent link fails at the
missing class. The same source's direct Foundation control links and executes
real activity initialization, title storage, invalidation and runtime class
identity (exit0, exact-prefix shutdown0). A private candidate library relink
and the corresponding umbrella-only fixture link succeed. Dynamic dlsym alone
would search transitive dependencies and would not prove two-level forwarding.
The integrated fixture and unchanged vendor retakes remain runtime gates.

Provenance: vendored public Foundation/NSUserActivity.h and existing real
Foundation source (rung2), original boundary fixture and vendor loader failures
(rung4/5 for import demand). This is an original compatibility forwarding policy;
no Apple implementation, SDK/binary exports or native addresses were inspected.
Activity handoff, indexing, notifications and service integration remain outside
this export-path test; existing provider limitations still apply.
