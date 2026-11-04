# Source manifest translated from lib/layout/Makefile.
netscape_archive(netscape_lay lib/layout lay
  bullet.c
  clipline.c
  layarena.c
  laycell.c
  laycols.c
  laydisp.c
  layembed.c
  layfind.c
  layform.c
  layfree.c
  laygrid.c
  layhrule.c
  layimage.c
  layinfo.c
  laylist.c
  laymap.c
  layout.c
  laysel.c
  layspace.c
  laysub.c
  laytable.c
  laytags.c
  laytext.c
  layutil.c
  ptinpoly.c
)
if(NETSCAPE_BUILD_WINDOWS_EDITOR)
 # Original BUILD_EDITOR source manifest from lib/layout/Makefile.
 target_sources(netscape_lay PRIVATE
  "${PROJECT_SOURCE_DIR}/lib/layout/layedit.c"
  "${PROJECT_SOURCE_DIR}/lib/layout/editor.cpp"
  "${PROJECT_SOURCE_DIR}/lib/layout/edtbuf.cpp"
  "${PROJECT_SOURCE_DIR}/lib/layout/edtsave.cpp"
  "${PROJECT_SOURCE_DIR}/lib/layout/edtcmd.cpp"
  "${PROJECT_SOURCE_DIR}/lib/layout/edtele.cpp"
  "${PROJECT_SOURCE_DIR}/lib/layout/edtutil.cpp"
  # Document colors are shared with the editor, without Mocha reflection.
  "${PROJECT_SOURCE_DIR}/lib/layout/laymocha.c")
 set_target_properties(netscape_lay PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
endif()
