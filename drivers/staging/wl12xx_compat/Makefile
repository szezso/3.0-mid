NOSTDINC_FLAGS := -I$(srctree)/drivers/staging/wl12xx_compat/include/ \
	-include linux/compat-2.6.h \
	$(CFLAGS) -DCOMPAT_BASE_TREE="\"dummy\"" \
	-DCOMPAT_BASE_TREE_VERSION="\"dummy\"" \
	-DCOMPAT_VERSION="\"dummy\"" \
	-DCOMPAT_PROJECT="\"Compat-wireless\""  \
	-DCONFIG_COMPAT_MAC80211_RC_DEFAULT="\"$(CONFIG_MAC80211_RC_DEFAULT)\""

obj-$(CONFIG_WL12XX_COMPAT_MENU) += compat/ net/wireless/ net/mac80211/ drivers/net/wireless/wl12xx/
obj-$(CONFIG_WL12XX_COMPAT_PLATFORM_DATA) += drivers/net/wireless/wl12xx/

