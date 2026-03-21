include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-mbusd
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-mbusd
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=LuCI support for mbusd Modbus gateway
  DEPENDS:=+luci-base +mbusd
  PKGARCH:=all
endef

define Package/luci-app-mbusd/description
  LuCI web interface for mbusd Modbus TCP to RTU/ASCII gateway
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

define Package/luci-app-mbusd/install
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view
	$(INSTALL_DATA) ./htdocs/luci-static/resources/view/mbusd.js \
		$(1)/www/luci-static/resources/view/mbusd.js

	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_DATA) ./root/usr/share/luci/menu.d/luci-app-mbusd.json \
		$(1)/usr/share/luci/menu.d/luci-app-mbusd.json

	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) ./root/usr/share/rpcd/acl.d/luci-app-mbusd.json \
		$(1)/usr/share/rpcd/acl.d/luci-app-mbusd.json

	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./root/etc/uci-defaults/luci-app-mbusd \
		$(1)/etc/uci-defaults/luci-app-mbusd
endef

$(eval $(call BuildPackage,luci-app-mbusd))
