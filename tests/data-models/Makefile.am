AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(LIBGDA_WFLAGS) \
	-DCHECK_FILES=\""$(top_srcdir)"\"

TESTS_ENVIRONMENT = GDA_TOP_SRC_DIR="$(abs_top_srcdir)" GDA_TOP_BUILD_DIR="$(abs_top_builddir)"
check_PROGRAMS = check_model_import check_virtual check_data_proxy check_model_copy check_pmodel check_empty_rs check_model_errors check_vcnc
TESTS = check_model_import check_virtual check_data_proxy check_model_copy check_pmodel check_empty_rs check_model_errors check_vcnc

common_sources = 

check_model_import_SOURCES = $(common_sources) check_model_import.c
check_model_import_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_virtual_SOURCES = $(common_sources) check_virtual.c
check_virtual_CFLAGS = \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/libgda/sqlite/virtual
check_virtual_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_data_proxy_SOURCES = $(common_sources) check_data_proxy.c
check_data_proxy_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_model_copy_SOURCES = $(common_sources) check_model_copy.c
check_model_copy_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_pmodel_SOURCES = $(common_sources) check_pmodel.c
check_pmodel_CFLAGS = \
	-I$(top_srcdir)/libgda/sqlite
check_pmodel_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(top_builddir)/tests/libgda-test-4.0.la \
	$(LIBGDA_LIBS)

check_empty_rs_SOURCES = $(common_sources) check_empty_rs.c
check_empty_rs_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_model_errors_SOURCES = $(common_sources) check_model_errors.c
check_model_errors_CFLAGS = \
	-I$(top_srcdir)/libgda/sqlite
check_model_errors_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(top_builddir)/tests/libgda-test-4.0.la \
	$(LIBGDA_LIBS)

check_vcnc_SOURCES = $(common_sources) check_vcnc.c
check_vcnc_CFLAGS = \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/libgda/sqlite/vcnc
check_vcnc_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)


EXTRA_DIST = \
	check_virtual.csv \
	city.csv \
	country.csv \
	data1.xml \
	pmodel_dbstruct.xml \
	pmodel_data_customers.xml \
	pmodel_data_locations.xml \
	cities1.xml \
	cities2.xml \
	cities3.xml \
	countriesA.xml \
	countriesB.xml

CLEANFILES = \
	pmodel.db pmodel.db-journal \
	vcnc.db vcnc.db-journal
