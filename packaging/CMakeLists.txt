INSTALL(PROGRAMS statefs-prerun DESTINATION bin)

set(STATEFS_LIB_DIR ${DST_LIB}/statefs)
configure_install(
  PROGRAMS provider-do install-provider loader-do provider-action loader-action
  DESTINATION ${STATEFS_LIB_DIR}
  )

if(ENABLE_USER_SESSION)
configure_install(
  FILES statefs.service
  DESTINATION ${SYSTEMD_USER_UNIT_DIR}
  )
endif(ENABLE_USER_SESSION)

configure_install(
  FILES statefs-system.service
  DESTINATION ${SYSTEMD_UNIT_DIR}
  )

install(PROGRAMS statefs-start DESTINATION ${STATEFS_LIB_DIR})
install(PROGRAMS statefs-stop DESTINATION ${STATEFS_LIB_DIR})
install(PROGRAMS once DESTINATION ${STATEFS_LIB_DIR})
