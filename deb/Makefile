
SSWAN_VERSION=5.7.2dr2
VERSION=$(SSWAN_VERSION)-vpp-1.0.0

DIR=sswan_$(VERSION)
TEST_DIR=sswan-vpp-test

prepare:
	mkdir $(DIR)
	cp -r DEBIAN $(DIR)
	cat sswan.files | xargs cp --parents -t $(DIR)
	cp ipsec.secrets $(DIR)/etc
	mkdir $(DIR)/etc/swanctl
	cp swanctl.conf $(DIR)/etc/swanctl

build:
	dpkg-deb -b $(DIR)

prepare-test:
	cp --parents /usr/local/bin/sswan_vpp_test $(TEST_DIR)

build-test:
	dpkg-deb -b $(TEST_DIR)

clean:
	rm -rf $(DIR)

.PHONY: clean prepare build build-test prepare-test
