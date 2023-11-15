SECTION = "libnmjson"
DESCRIPTION = "NON-Malloc Json library"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=96c723867664dad4ccc93d38a23879f7"

FILESEXTRAPATHS:prepend := "${THISDIR}/${PV}:"

SRC_URI = "\
  file://libnmjson-${PV}.tar.gz \
"

inherit autotools

S = "${WORKDIR}/libnmjson-${PV}"

do_install:append(){
	#現状、普通のinstallではヘッダを入れることができなかったので、手動コピー
	mkdir -p ${D}/${includedir}
	cp -Rf ${S}/include/nmjson ${D}/${includedir}/
}

__BB_DONT_CACHE = "1"
