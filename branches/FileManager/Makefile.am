INCLUDES = $(WX_CXXFLAGS) \
		-I$(top_srcdir)/src/include \
		-I$(top_srcdir)/src/include/wxscintilla/include \
		-I$(top_srcdir)/src/include/wxFlatNotebook/include \
		-I/usr/include/gnome-vfs-2.0 \
		-I/usr/include/glib-2.0 \
		-I/usr/lib/glib-2.0/include


pluginlibdir = $(pkglibdir)/plugins
pkgdata_DATA = FileManager.zip

pluginlib_LTLIBRARIES = libFileManager.la

libFileManager_la_LDFLAGS = -module -version-info 0:1:0 -shared -no-undefined -avoid-version

libFileManager_la_LIBADD = $(PLUGIN_WX_LIBS) ../../../sdk/libcodeblocks.la /usr/lib/libgnomevfs-2.la

libFileManager_la_SOURCES = FileManager.cpp FileExplorer.cpp FileExplorerSettings.cpp FileExplorerUpdater.cpp directorymonitor.cpp

noinst_HEADERS = FileManager.h FileExplorer.h FileExplorerSettings.h FileExplorerUpdater.h directorymonitor.h

EXTRA_DIST = FileManager.cbp FileManager-unix.cbp Resources/fileexplorersettings.xrc manifest.xml

CLEANFILES = $(pkgdata_DATA)

FileManager.zip: manifest.xml Resources/fileexplorersettings.xrc
	PWD=`pwd` cd $(srcdir) && zip $(PWD)/FileManager.zip manifest.xml Resources/fileexplorersettings.xrc > /dev/null
