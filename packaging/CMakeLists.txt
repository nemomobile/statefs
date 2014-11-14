INSTALL(PROGRAMS statefs-prerun DESTINATION bin)

configure_file(provider-do.in provider-do @ONLY)
configure_file(install-provider.in install-provider @ONLY)
configure_file(loader-do.in loader-do @ONLY)
configure_file(provider-action.in provider-action @ONLY)
configure_file(loader-action.in loader-action @ONLY)

configure_file(statefs.service.in statefs.service @ONLY)
configure_file(statefs-system.service.in statefs-system.service @ONLY)

INSTALL(PROGRAMS provider-do DESTINATION ${DST_LIB}/statefs)
INSTALL(PROGRAMS install-provider DESTINATION ${DST_LIB}/statefs)
INSTALL(PROGRAMS loader-do DESTINATION ${DST_LIB}/statefs)
INSTALL(PROGRAMS provider-action DESTINATION ${DST_LIB}/statefs)
INSTALL(PROGRAMS loader-action DESTINATION ${DST_LIB}/statefs)
INSTALL(PROGRAMS statefs-start DESTINATION ${DST_LIB}/statefs)
INSTALL(PROGRAMS statefs-stop DESTINATION ${DST_LIB}/statefs)
INSTALL(PROGRAMS once DESTINATION ${DST_LIB}/statefs)
