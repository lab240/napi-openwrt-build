include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-mbpoll
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_MAINTAINER:=NapiLab <info@napiworld.ru>
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-mbpoll
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=Modbus Poll Test Tool
  DEPENDS:=+luci-base +mbpoll +mbscan
  PKGARCH:=all
endef

define Package/luci-app-mbpoll/description
  Web interface for manual Modbus RTU/TCP device polling.
  Allows reading Holding Registers (FC03) and Input Registers (FC04)
  with data type interpretation (uint16, int16, float32, uint32).
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

define Package/luci-app-mbpoll/install
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_DATA) ./root/usr/share/luci/menu.d/luci-app-mbpoll.json $(1)/usr/share/luci/menu.d/

	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) ./root/usr/share/rpcd/acl.d/luci-app-mbpoll.json $(1)/usr/share/rpcd/acl.d/

	$(INSTALL_DIR) $(1)/www/luci-static/resources/view
	$(INSTALL_DATA) ./htdocs/luci-static/resources/view/mbpoll.js $(1)/www/luci-static/resources/view/
endef

$(eval $(call BuildPackage,luci-app-mbpoll))
