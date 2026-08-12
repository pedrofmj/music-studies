if(NOT DEFINED UNIT)
    message(FATAL_ERROR "UNIT is required")
endif()
file(READ "${UNIT}" CONTENT)

set(REQUIRED
    "ConditionPathExists=%E/music-rig/shadow-enabled"
    "ExecStartPre=%h/.local/bin/music-rigd resolve-paths --check-only"
    "ExecStart=%h/.local/bin/music-rigd run-shadow --output-suppressed"
    "StandardOutput=null"
    "StandardError=journal"
    "NoNewPrivileges=true"
    "WantedBy=default.target"
)
foreach(TEXT IN LISTS REQUIRED)
    string(FIND "${CONTENT}" "${TEXT}" INDEX)
    if(INDEX EQUAL -1)
        message(FATAL_ERROR "Systemd shadow unit omitted: ${TEXT}")
    endif()
endforeach()

set(FORBIDDEN
    "airstar-live-setup"
    "pw-link"
    "jack_connect"
    "systemctl"
)
foreach(TEXT IN LISTS FORBIDDEN)
    string(FIND "${CONTENT}" "${TEXT}" INDEX)
    if(NOT INDEX EQUAL -1)
        message(FATAL_ERROR "Systemd shadow unit contains forbidden text: ${TEXT}")
    endif()
endforeach()
