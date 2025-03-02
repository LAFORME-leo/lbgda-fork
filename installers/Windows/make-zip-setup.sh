#!/bin/bash

#
#
# This script creates a .ZIP file containing a minimum set of files required
# to execute the GDA SQL console.
#
# It also creates sevenal .nsh files meant to be used by the gda-browser.nsi file
# to create a GdaBrowser installer file with NSIS
#
# It is made to be used when cross compiling
#
# Variables to set are:
# $depend_path is the location of the cross compilation environment and dependencies (GLib, DBMS client libraries, etc)
# $prefix is the location of the compiled and installed Libgda's sources
# $version is the current Libgda's version
#

depend_path="/usr/i686-pc-mingw32/sys-root/mingw /local/Win32Compiled /local/Win32/bdb /local/Win32/mysql /local/Win32/pgsql /local/Win32/ldap /local/Win32/mdb /local/Win32/oracle"
prefix=/local/Win32/Libgda
debug=no

#
# no modification below
#
#
# determine version
#
current_dir=`pwd`
conf=$current_dir/../../configure.ac
if test -e $conf
then
    major=`cat $conf | grep m4_define | grep major | sed -e 's/[^ ]* \([0-9]*\).*/\1/'`
    minor=`cat $conf | grep m4_define | grep minor | sed -e 's/[^ ]* \([0-9]*\).*/\1/'`
    micro=`cat $conf | grep m4_define | grep micro | sed -e 's/[^ ]* \([0-9]*\).*/\1/'`
    version="$major.$minor.$micro"
    echo "Determined version [$version]"
else
    echo "configure.ac file not found"
fi

archive=${current_dir}/libgda-${version}.zip
archive_dev=${current_dir}/libgda-dev-${version}.zip
archive_ext=${current_dir}/libgda-dep-${version}.zip
nshfiles=(core.nsh prov_bdb.nsh prov_mdb.nsh prov_mysql.nsh prov_oracle.nsh prov_postgresql.nsh prov_sqlite.nsh prov_web.nsh prov_ldap.nsh)
tmpfile=`mktemp`

# remove current archive if it exists
rm -f $archive $archive_dev $archive_ext
rm -f *.nsh *.exe
rm -f gda-browser.nsi

if test "$CLEAN" = "yes"
then
    exit 0
fi

#
# Takes 3 arguments:
# $1 = the archive name
# $2 = the directory(ies) where files may be located
# $3 = the prefix under $2 where all the files are
# $4 = a list of files
#
function add_files_to_zip
{
    local arch=$1
    local dir=$2
    local subdir=$3
    files=$4

    local localtmpfile=`mktemp`
    rm -f $tmpfile

    for item in ${files[*]}
    do
	rm -f $localtmpfile
	echo "Zipping file [$subdir] [$item]"
	for tdir in ${dir[*]}
	do
	    pushd $tdir >&/dev/null
	    if ! test -e $subdir
	    then
		continue
	    fi

	    find $subdir -name $item 2> /dev/null >> $localtmpfile
	    if test -s $localtmpfile
	    then
		if test "x$debug" = "xyes"
		then
		    cat $localtmpfile | zip -@ $arch
		else
		    cat $localtmpfile | zip -@ $arch >& /dev/null
		fi
		if [ $? -ne 0 ]
		then
		    echo "Error adding to ZIP..."
		    exit 1
		fi
		if test "x$debug" = "xyes"
		then
		    echo " found in $tdir"
		fi
		popd >& /dev/null
		break;
	    fi
	    popd >& /dev/null
	done

	if test -s $localtmpfile
	then
	    cat $localtmpfile | sed -e "s,^,${tdir}/," >> $tmpfile
	else
	    echo "File not found, searched in $dir"
	    exit 1
	fi
    done

    rm -f $localtmpfile
}


#
# Takes 3 arguments:
# $1 = the archive name
# $2 = the prefix directory where files are located
# $3 = the prefix under $2 where all the files are
#
function add_all_files_to_zip
{
    local arch=$1
    local dir=$2
    local subdir=$3

    rm -f $tmpfile
    echo "Zipping all files in directory [$subdir]"
    for tdir in ${dir[*]}
    do
	pushd $tdir >&/dev/null
	if test -e $subdir
	then
	    echo $subdir > $tmpfile
	    zip ${arch} $subdir/* >& /dev/null
	    if [ $? -ne 0 ]
	    then
		echo "Error adding to ZIP..."
		exit 1
	    fi
	    if test "x$debug" = "xyes"
	    then
		echo " found in $tdir"
	    fi
	    popd >& /dev/null
	    break;
	fi
	popd >& /dev/null
    done

    if test -s $tmpfile
    then
	cat $tmpfile | sed -e "s,^,${tdir}/," >> $tmpfile.1
	mv $tmpfile.1 $tmpfile
    else
	echo "File not found, searched in $dir"
	exit 1
    fi
}

# test NSH file is known
function check_nsh_file_ok
{
    file=$1
    found="no"
    for item in ${nshfiles[*]}
    do
	if test $item == "$file"
	then
	    found="yes"
	fi
    done
    if test $found == "no"
    then
	echo "NSH file $file not known"
	exit 1
    fi
}

#
# Takes 1 argument:
# $1 = the section name, without the .nsh extension
# $2 = the prefix under where all the files will be installed
#
function add_found_files_to_nsh
{
    section=$1
    subdir=$2

    check_nsh_file_ok $section.nsh
    if test "x$debug" = "xyes"
    then
	echo "Adding file(s) in section '$section.nsh'"
	echo "==== The following file(s) are added to NSH file ===="
	cat $tmpfile
	echo "====================================================="
    fi

    wsubdir=`echo "${subdir}" | sed -e 's/\//\\\\/g'`
    echo "  SetOutPath \"\$INSTDIR\\${wsubdir}\"" >> $section.nsh
    #cat $tmpfile | sed -e 's/^/  File "/' -e 's/$/"/' >> $section.nsh
    while read line
    do
	if test -d $line
	then
	    echo "$line/*" | sed -e 's/^/  File "/' -e 's/$/"/' >> $section.nsh
	else
	    echo "$line" | sed -e 's/^/  File "/' -e 's/$/"/' >> $section.nsh
	fi
    done < $tmpfile
}

#
# initialize NSH sections
#
cat > core.nsh <<EOF
Section "GdaBrowser & Libgda" SEC01
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
  SectionIn 1 RO
  CreateDirectory "\$SMPROGRAMS\GdaBrowser"
  CreateShortCut "\$SMPROGRAMS\GdaBrowser\GdaBrowser.lnk" "\$INSTDIR\bin\gda-browser-4.0.exe"
  CreateShortCut "\$DESKTOP\GdaBrowser.lnk" "\$INSTDIR\bin\gda-browser-4.0.exe"
EOF

cat > prov_mysql.nsh <<EOF
Section "MySQL" SEC02
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF


cat > prov_bdb.nsh <<EOF
Section /o "Berkeley Db" SEC03
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > prov_mdb.nsh <<EOF
Section "MS Access" SEC04
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > prov_postgresql.nsh <<EOF
Section "PostgreSQL" SEC05
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > prov_oracle.nsh <<EOF
Section /o "Oracle" SEC06
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > prov_sqlite.nsh <<EOF
Section "SQLite" SEC07
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > prov_web.nsh <<EOF
Section /o "Web" SEC08
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > prov_ldap.nsh <<EOF
Section "Ldap" SEC09
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > config.nsh <<EOF
!define PRODUCT_VERSION "$version"
EOF

cat > uninst.nsh <<EOF
Section Uninstall
  Delete "\$INSTDIR\\\${PRODUCT_NAME}.url"
  Delete "\$INSTDIR\uninst.exe"

  RMDir /r "\$SMPROGRAMS\GdaBrowser"
  Delete "\$DESKTOP\GdaBrowser.lnk"

  RMDir /r "\$INSTDIR\bin"
  RMDir /r "\$INSTDIR\lib"
  RMDir /r "\$INSTDIR\etc"
  RMDir /r "\$INSTDIR\share"
  RMDir "\$INSTDIR"

  DeleteRegKey \${PRODUCT_UNINST_ROOT_KEY} "\${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "\${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd
EOF

#
# dependencies DLLs
#
files=(libexpat.dll libgio-2.*.dll libglib-2.*.dll libgmodule-2.*.dll libgobject-2.*.dll libgthread-2.*.dll libxml2*.dll libsoup-2.*.dll libgdk_pixbuf-2.*.dll libgdk-win32-*.dll libgtk-win32-*.dll libatk-1.*.dll libpng*.dll libpango-1.*.dll libpangocairo-1.*.dll libpangoft2-1.*.dll libpangowin32-1.*.dll libcairo-2.dll libcairo-gobject-2.dll libfontconfig-1.dll libgoocanvas-*.dll libcdt*.dll libcgraph*.dll libgvc*.dll libpathplan*.dll libxdot*.dll libfreetype-6.dll libintl-8.dll libpixman-1-0.dll libjasper-1.dll libjpeg*.dll libtiff*.dll libffi*.dll readline.dll iconv.dll libgraph*.dll libtermcap*.dll)
add_files_to_zip $archive_ext "${depend_path}" bin $files
add_found_files_to_nsh core bin

files=(libdb51.dll)
add_files_to_zip $archive_ext "${depend_path}" bin $files
add_found_files_to_nsh prov_bdb bin

files=(libmySQL.dll)
add_files_to_zip $archive_ext "${depend_path}" bin $files
add_found_files_to_nsh prov_mysql bin

files=(libeay32.dll libpq.dll libxml2.dll libxslt.dll msvcr71.dll ssleay32.dll)
add_files_to_zip $archive_ext "${depend_path}" bin $files
add_found_files_to_nsh prov_postgresql bin

files=(liblber.dll libldap.dll)
add_files_to_zip $archive_ext "${depend_path}" bin $files
add_found_files_to_nsh prov_ldap bin

#
# dependencies from the cross compilation environment
#

#
# Libgda's files
#
files=(information_schema.xml import_encodings.xml)
add_files_to_zip $archive $prefix share/libgda-4.0 $files
add_found_files_to_nsh core share/libgda-4.0

files=(gda-sql.lang)
add_files_to_zip $archive $prefix share/libgda-4.0/language-specs $files
add_found_files_to_nsh core share/libgda-4.0/language-specs

files=(bdb_specs_dsn.xml)
add_files_to_zip $archive $prefix share/libgda-4.0 $files
add_found_files_to_nsh prov_bdb share/libgda-4.0

files=(mdb_specs_dsn.xml)
add_files_to_zip $archive $prefix share/libgda-4.0 $files
add_found_files_to_nsh prov_mdb share/libgda-4.0

files=(mysql_specs_add_column.xml mysql_specs_create_db.xml mysql_specs_create_index.xml mysql_specs_create_table.xml mysql_specs_create_view.xml mysql_specs_drop_column.xml mysql_specs_drop_db.xml mysql_specs_drop_index.xml mysql_specs_drop_table.xml mysql_specs_drop_view.xml mysql_specs_dsn.xml mysql_specs_rename_table.xml)
add_files_to_zip $archive $prefix share/libgda-4.0 $files
add_found_files_to_nsh prov_mysql share/libgda-4.0

files=(postgres_specs_add_column.xml postgres_specs_create_db.xml postgres_specs_create_index.xml postgres_specs_create_table.xml postgres_specs_create_view.xml postgres_specs_drop_column.xml postgres_specs_drop_db.xml postgres_specs_drop_index.xml postgres_specs_drop_table.xml postgres_specs_drop_view.xml postgres_specs_dsn.xml postgres_specs_rename_table.xml)
add_files_to_zip $archive $prefix share/libgda-4.0 $files
add_found_files_to_nsh prov_postgresql share/libgda-4.0

files=(sqlite_specs_add_column.xml sqlite_specs_create_db.xml sqlite_specs_create_index.xml sqlite_specs_create_table.xml sqlite_specs_create_view.xml sqlite_specs_drop_db.xml sqlite_specs_drop_index.xml sqlite_specs_drop_table.xml sqlite_specs_drop_view.xml sqlite_specs_dsn.xml sqlite_specs_rename_table.xml)
add_files_to_zip $archive $prefix share/libgda-4.0 $files
add_found_files_to_nsh prov_sqlite share/libgda-4.0

files=(web_specs_auth.xml web_specs_dsn.xml)
add_files_to_zip $archive $prefix share/libgda-4.0 $files
add_found_files_to_nsh prov_web share/libgda-4.0

files=(oracle_specs_dsn.xml oracle_specs_create_table.xml)
add_files_to_zip $archive $prefix share/libgda-4.0 $files
add_found_files_to_nsh prov_oracle share/libgda-4.0

files=(ldap_specs_auth.xml ldap_specs_dsn.xml)
add_files_to_zip $archive $prefix share/libgda-4.0 $files
add_found_files_to_nsh prov_ldap share/libgda-4.0

files=(gdaui-generic.png)
add_files_to_zip $archive $prefix share/libgda-4.0/pixmaps $files
add_found_files_to_nsh core share/libgda-4.0/pixmaps


#
#copy some Gnome files to be installed on Windows
#
cp /usr/share/icons/gnome/16x16/actions/bookmark-new.png $prefix/share/libgda-4.0/icons/hicolor/16x16/actions
cp /usr/share/icons/gnome/22x22/actions/bookmark-new.png $prefix/share/libgda-4.0/icons/hicolor/22x22/actions
cp /usr/share/icons/gnome/24x24/actions/bookmark-new.png $prefix/share/libgda-4.0/icons/hicolor/24x24/actions
cp /usr/share/icons/gnome/32x32/actions/bookmark-new.png $prefix/share/libgda-4.0/icons/hicolor/32x32/actions
cp /usr/share/icons/gnome/16x16/actions/window-new.png $prefix/share/libgda-4.0/icons/hicolor/16x16/actions
cp /usr/share/icons/gnome/22x22/actions/window-new.png $prefix/share/libgda-4.0/icons/hicolor/22x22/actions
cp /usr/share/icons/gnome/24x24/actions/window-new.png $prefix/share/libgda-4.0/icons/hicolor/24x24/actions
cp /usr/share/icons/gnome/32x32/actions/window-new.png $prefix/share/libgda-4.0/icons/hicolor/32x32/actions
mkdir -p $prefix/share/libgda-4.0/icons/hicolor/16x16/apps
mkdir -p $prefix/share/libgda-4.0/icons/hicolor/22x22/apps
mkdir -p $prefix/share/libgda-4.0/icons/hicolor/24x24/apps
mkdir -p $prefix/share/libgda-4.0/icons/hicolor/32x32/apps
cp /usr/share/icons/gnome/16x16/apps/accessories-text-editor.png $prefix/share/libgda-4.0/icons/hicolor/16x16/apps
cp /usr/share/icons/gnome/22x22/apps/accessories-text-editor.png $prefix/share/libgda-4.0/icons/hicolor/22x22/apps
cp /usr/share/icons/gnome/24x24/apps/accessories-text-editor.png $prefix/share/libgda-4.0/icons/hicolor/24x24/apps
cp /usr/share/icons/gnome/32x32/apps/accessories-text-editor.png $prefix/share/libgda-4.0/icons/hicolor/32x32/apps

add_all_files_to_zip $archive $prefix share/libgda-4.0/pixmaps
add_found_files_to_nsh core share/libgda-4.0/pixmaps
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/16x16/actions
add_found_files_to_nsh core share/libgda-4.0/icons/hicolor/16x16/actions
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/22x22/actions
add_found_files_to_nsh core share/libgda-4.0/icons/hicolor/22x22/actions
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/24x24/actions
add_found_files_to_nsh core share/libgda-4.0/icons/hicolor/24x24/actions
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/32x32/actions
add_found_files_to_nsh core share/libgda-4.0/icons/hicolor/32x32/actions
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/scalable/actions
add_found_files_to_nsh core share/libgda-4.0/icons/hicolor/scalable/actions
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/16x16/apps
add_found_files_to_nsh core share/libgda-4.0/icons/hicolor/16x16/apps
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/22x22/apps
add_found_files_to_nsh core share/libgda-4.0/icons/hicolor/22x22/apps
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/24x24/apps
add_found_files_to_nsh core share/libgda-4.0/icons/hicolor/24x24/apps
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/32x32/apps
add_found_files_to_nsh core share/libgda-4.0/icons/hicolor/32x32/apps

files=(gda-browser-4.0.png)
add_files_to_zip $archive $prefix share/pixmaps $files
add_found_files_to_nsh core share/pixmaps

files=(gda-control-center.png)
add_files_to_zip $archive $prefix share/libgda-4.0/pixmaps $files
add_found_files_to_nsh core share/libgda-4.0/pixmaps

files=(gdaui-entry-number.xml gdaui-entry-string.xml)
add_files_to_zip $archive $prefix share/libgda-4.0/ui $files
add_found_files_to_nsh core share/libgda-4.0/ui

files=(cnc.js md5.js jquery.js mouseapp_2.js mouseirb_2.js irb.js gda.css gda-print.css irb.css)
add_files_to_zip $archive $prefix share/libgda-4.0/web $files
add_found_files_to_nsh core share/libgda-4.0/web

files=(libgda-paramlist.dtd libgda-array.dtd libgda-server-operation.dtd gdaui-layout.dtd)
add_files_to_zip $archive $prefix share/libgda-4.0/dtd $files
add_found_files_to_nsh core share/libgda-4.0/dtd

files=(config sales_test.db)
add_files_to_zip $archive $prefix etc/libgda-4.0 $files
add_found_files_to_nsh core etc/libgda-4.0

files=(gtk.immodules)
add_files_to_zip $archive_ext "${depend_path}" etc/gtk-2.0 $files
add_found_files_to_nsh core etc/gtk-2.0

files=(gtkrc)
add_files_to_zip $archive_ext . etc/gtk-2.0 $files
add_found_files_to_nsh core etc/gtk-2.0

files=(pango.modules)
add_files_to_zip $archive_ext "${depend_path}" etc/pango $files
add_found_files_to_nsh core etc/pango

files=(pango-*.dll)
add_files_to_zip $archive_ext "${depend_path}" lib/pango/1.6.0/modules $files
add_found_files_to_nsh core lib/pango/1.6.0/modules

files=(gda-sql-4.0.exe libgda-4.0-4.dll libgda-report-4.0-4.dll libgda-ui-4.0-4.dll gda-browser-4.0.exe gda-control-center-4.0.exe)
add_files_to_zip $archive $prefix bin $files
add_found_files_to_nsh core bin

add_all_files_to_zip $archive $prefix share/gnome/help/gda-sql/C

files=(gda-test-connection-4.0.exe)
add_files_to_zip $archive $prefix bin $files

files=(gspawn-win32-helper.exe)
add_files_to_zip $archive "${depend_path}" bin $files
add_found_files_to_nsh core bin

files=(gdaui-demo-4.0.exe)
add_files_to_zip $archive_dev $prefix bin $files

files=(libgda-bdb.dll)
add_files_to_zip $archive $prefix lib/libgda-4.0/providers $files
add_found_files_to_nsh prov_bdb lib/libgda-4.0/providers

files=(libgda-mdb.dll)
add_files_to_zip $archive $prefix lib/libgda-4.0/providers $files
add_found_files_to_nsh prov_mdb lib/libgda-4.0/providers

files=(libgda-mysql.dll)
add_files_to_zip $archive $prefix lib/libgda-4.0/providers $files
add_found_files_to_nsh prov_mysql lib/libgda-4.0/providers

files=(libgda-postgres.dll)
add_files_to_zip $archive $prefix lib/libgda-4.0/providers $files
add_found_files_to_nsh prov_postgresql lib/libgda-4.0/providers

files=(libgda-sqlite.dll)
add_files_to_zip $archive $prefix lib/libgda-4.0/providers $files
add_found_files_to_nsh prov_sqlite lib/libgda-4.0/providers

files=(libgda-web.dll)
add_files_to_zip $archive $prefix lib/libgda-4.0/providers $files
add_found_files_to_nsh prov_web lib/libgda-4.0/providers

files=(libgda-ldap.dll)
add_files_to_zip $archive $prefix lib/libgda-4.0/providers $files
add_found_files_to_nsh prov_ldap lib/libgda-4.0/providers

files=(libgda-oracle.dll)
add_files_to_zip $archive $prefix lib/libgda-4.0/providers $files
add_found_files_to_nsh prov_oracle lib/libgda-4.0/providers

files=(gdaui-entry-filesel-spec.xml gdaui-entry-password.xml gdaui-entry-pict-spec.xml gdaui-entry-pict-spec_string.xml libgda-ui-plugins.dll)
add_files_to_zip $archive $prefix lib/libgda-4.0/plugins $files
add_found_files_to_nsh core lib/libgda-4.0/plugins

files=(libwimp.dll)
add_files_to_zip $archive_ext "${depend_path}" lib/gtk-2.0/2.10.0/engines $files
add_found_files_to_nsh core lib/gtk-2.0/2.10.0/engines

#
# includes
#
files=(gda-attributes-manager.h gda-batch.h gda-binreloc.h gda-blob-op.h gda-column.h gda-config.h gda-connection-event.h gda-connection.h gda-connection-private.h gda-data-access-wrapper.h gda-data-comparator.h gda-data-handler.h gda-data-model-array.h gda-data-model-bdb.h gda-data-model-dir.h gda-data-model-extra.h gda-data-model.h gda-data-model-import.h gda-data-model-iter-extra.h gda-data-model-iter.h gda-data-model-private.h gda-data-proxy.h gda-data-select.h gda-debug-macros.h gda-decl.h gda-enums.h gda-enum-types.h gda-holder.h gda-lockable.h gda-log.h gda-marshal.h gda-meta-store.h gda-meta-struct.h gda-mutex.h gda-quark-list.h gda-row.h gda-server-operation.h gda-server-provider-extra.h gda-server-provider.h gda-server-provider-private.h gda-set.h gda-statement-extra.h gda-statement.h gda-transaction-status.h gda-transaction-status-private.h gda-util.h gda-value.h gda-xa-transaction.h libgda.h libgda-global-variables.h gda-repetitive-statement.h gda-sql-builder.h gda-tree.h gda-tree-manager.h gda-tree-mgr-columns.h gda-tree-mgr-label.h gda-tree-mgr-schemas.h gda-tree-mgr-select.h gda-tree-mgr-tables.h gda-tree-node.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda $files

files=(gda-sqlite-provider.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/sqlite $files

files=(gda-thread-wrapper.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/thread-wrapper $files

files=(gda-handler-bin.h gda-handler-boolean.h gda-handler-numerical.h gda-handler-string.h gda-handler-time.h gda-handler-type.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/handlers $files

files=(gda-report-docbook-document.h gda-report-document.h gda-report-engine.h gda-report-rml-document.h libgda-report.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda-report $files

files=(gda-data-select-priv.h gda-pstmt.h gda-meta-column-types.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/providers-support $files

files=(gda-sql-parser-enum-types.h gda-sql-parser.h gda-sql-statement.h gda-statement-struct-compound.h gda-statement-struct-decl.h gda-statement-struct-delete.h gda-statement-struct.h gda-statement-struct-insert.h gda-statement-struct-parts.h gda-statement-struct-pspec.h gda-statement-struct-select.h gda-statement-struct-trans.h gda-statement-struct-unknown.h gda-statement-struct-update.h gda-statement-struct-util.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/sql-parser $files

files=(gda-vconnection-data-model.h gda-vconnection-hub.h gda-virtual-connection.h gda-virtual-provider.h gda-vprovider-data-model.h gda-vprovider-hub.h libgda-virtual.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/virtual $files

files=(gdaui-basic-form.h gdaui-data-entry.h gdaui-data-selector.h gdaui-enums.h gdaui-login.h gdaui-raw-grid.h gdaui-cloud.h gdaui-data-filter.h gdaui-data-store.h gdaui-enum-types.h gdaui-plugin.h gdaui-server-operation.h gdaui-combo.h gdaui-data-proxy.h gdaui-decl.h gdaui-form.h gdaui-provider-selector.h gdaui-tree-store.h gdaui-data-cell-renderer-util.h gdaui-data-proxy-info.h gdaui-easy.h gdaui-grid.h gdaui-raw-form.h gdaui-rt-editor.h libgda-ui.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda-ui $files

#
# PC files
#
add_all_files_to_zip $archive_dev $prefix lib/pkgconfig

#
# static libs
#
files=(libgda-4.0.dll.a libgda-4.0.lib libgda-4.0.def libgda-report-4.0.dll.a libgda-report-4.0.lib libgda-report-4.0.def libgda-ui-4.0.dll.a libgda-ui-4.0.lib libgda-ui-4.0.def)
add_files_to_zip $archive_dev $prefix lib $files

#
# demo
#
files=(basic_form.c cloud.c combo.c custom_layout.xml data_model_dir.c ddl_queries.c demo_db.db form.c form_data_layout.c form_pict.c form_rw.c grid.c grid_data_layout.c grid_pict.c grid_rw.c linked_grid_form.c linked_model_param.c login.c provider_sel.c tree.c)
add_files_to_zip $archive_dev $prefix share/libgda-4.0/demo $files

#
# doc
#
#add_all_files_to_zip $archive_dev $prefix share/gtk-doc/html/libgda-4.0

#
# translations
#
add_all_files_to_zip $archive $prefix share/locale

#
# end NSH files
#
for item in ${nshfiles[*]}
do
    echo "SectionEnd" >> $item
done

cat gda-browser-tmpl.nsi | sed -e "s,GdaBrowserSetup,GdaBrowserSetup-${version}," > gda-browser.nsi

#
# The End
#
rm -f $tmpfile
echo "Archives written to:"
echo "   ${archive}"
echo "   ${archive_dev}"
echo "   ${archive_ext}"
echo "Generate Windows installer using 'makensis -V0 gda-browser.nsi'"
