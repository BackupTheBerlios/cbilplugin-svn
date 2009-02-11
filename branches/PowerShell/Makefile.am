INCLUDES = $(WX_CXXFLAGS) \
		-I$(top_srcdir)/src/include \
		-I$(top_srcdir)/src/include/wxscintilla/include \
		-I$(top_srcdir)/src/include/wxFlatNotebook/include

pluginlibdir = $(pkglibdir)/plugins
pkgdata_DATA = PowerShell.zip

pluginlib_LTLIBRARIES = libPowerShell.la

libPowerShell_la_LDFLAGS = -module -version-info 0:1:0 -shared -no-undefined -avoid-version

libPowerShell_la_LIBADD = $(PLUGIN_WX_LIBS) ../../../sdk/libcodeblocks.la

libPowerShell_la_SOURCES = PowerShell.cpp CmdConfigDialog.cpp PipedProcessCtrl.cpp se_globals.cpp ShellCtrlBase.cpp shellproperties.cpp

noinst_HEADERS = PowerShell.h CmdConfigDialog.h PipedProcessCtrl.h se_globals.h ShellCtrlBase.h shellproperties.h

EXTRA_DIST = PowerShell.cbp PowerShell-unix.cbp

CLEANFILES = $(pkgdata_DATA)

PowerShell.zip: manifest.xml
	PWD=`pwd` cd $(srcdir) && zip -j9 $(PWD)/PowerShell.zip manifest.xml > /dev/null

