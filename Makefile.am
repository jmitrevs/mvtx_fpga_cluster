AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = \
  -I$(includedir) \
  -I$(OFFLINE_MAIN)/include \
  -isystem$(ROOTSYS)/include \
  -I/sphenix/user/cdean/software/vcpkg/installed/x64-linux/include

AM_LDFLAGS = \
  -L$(libdir) \
  -L$(OFFLINE_MAIN)/lib \
  -L$(OFFLINE_MAIN)/lib64 \
  -L/sphenix/user/cdean/software/vcpkg/installed/x64-linux/lib

pkginclude_HEADERS = \
  mvtxFPGAclustering.h

lib_LTLIBRARIES = \
  libmvtxFPGAclustering.la

libmvtxFPGAclustering_la_SOURCES = \
  mvtxFPGAclustering.cc

libmvtxFPGAclustering_la_LIBADD = \
  -lphool \
  -ltrack_io \
  -lg4detectors \
  -lmvtx \
  -lSubsysReco \
  -ljsoncpp

BUILT_SOURCES = testexternals.cc

noinst_PROGRAMS = \
  testexternals

testexternals_SOURCES = testexternals.cc
testexternals_LDADD   = libmvtxFPGAclustering.la

testexternals.cc:
	echo "//*** this is a generated file. Do not commit, do not edit" > $@
	echo "int main()" >> $@
	echo "{" >> $@
	echo "  return 0;" >> $@
	echo "}" >> $@

clean-local:
	rm -f $(BUILT_SOURCES)
