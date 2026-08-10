if(NOT DEFINED OBSERVER)
    message(FATAL_ERROR "OBSERVER is required")
endif()

file(READ "${OBSERVER}" CONTENTS)

set(FORBIDDEN_TOKENS
    "pw-link"
    "wpctl"
    "pw-cli"
    "systemctl --user start"
    "systemctl --user stop"
    "systemctl --user restart"
    "systemctl --user enable"
    "systemctl --user disable"
    "kill "
    "rm "
    "mv "
    "cp "
    "install "
)

foreach(FORBIDDEN_TOKEN IN LISTS FORBIDDEN_TOKENS)
    string(FIND "${CONTENTS}" "${FORBIDDEN_TOKEN}" TOKEN_INDEX)
    if(NOT TOKEN_INDEX EQUAL -1)
        message(FATAL_ERROR
            "Read-only observer contains forbidden token: ${FORBIDDEN_TOKEN}"
        )
    endif()
endforeach()
