project(launchd-top)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_subdirectory(liblaunch)
add_subdirectory(src)
add_subdirectory(support)

install(FILES man/launchctl.1 DESTINATION libexec/darling/usr/share/man/man1)
install(FILES man/launchd.8 DESTINATION libexec/darling/usr/share/man/man8)

# launchctl rejects group/world-writable job directories before scanning them.
# Directories implicitly created by plist installs otherwise inherit the umask.
foreach(domain Library System/Library)
	foreach(kind LaunchDaemons LaunchAgents)
		install(DIRECTORY DESTINATION libexec/darling/${domain}/${kind}
			DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
				GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
	endforeach()
endforeach()
