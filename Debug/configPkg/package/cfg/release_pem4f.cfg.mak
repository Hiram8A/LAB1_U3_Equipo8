# invoke SourceDir generated makefile for release.pem4f
release.pem4f: .libraries,release.pem4f
.libraries,release.pem4f: package/cfg/release_pem4f.xdl
	$(MAKE) -f C:\Users\HIM8A\workspace_v12\LAB1_U3\Aux_files/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\HIM8A\workspace_v12\LAB1_U3\Aux_files/src/makefile.libs clean

