INSTALL(PROGRAMS statefs-prerun DESTINATION bin)

configure_file(provider-do.in provider-do @ONLY)
configure_file(install-provider.in install-provider @ONLY)
configure_file(loader-do.in loader-do @ONLY)

INSTALL(PROGRAMS provider-do DESTINATION ${DST_LIB}/statefs)
INSTALL(PROGRAMS install-provider DESTINATION ${DST_LIB}/statefs)
INSTALL(PROGRAMS loader-do DESTINATION ${DST_LIB}/statefs)
