# Source manifest translated from mocha/src/Makefile.
netscape_archive(netscape_mocha mocha/src mocha
  mo_array.c
  mo_atom.c
  mo_bcode.c
  mo_bool.c
  mo_cntxt.c
  mo_date.c
  mo_emit.c
  mo_fun.c
  mo_java.c
  mo_math.c
  mo_num.c
  mo_obj.c
  mo_parse.c
  mo_scan.c
  mo_scope.c
  mo_str.c
  mocha.c
  mochaapi.c
  mochalib.c
)
set_target_properties(netscape_mocha PROPERTIES PREFIX "")
