wl12xx_compat-objs		= main.o cmd.o io.o event.o tx.o rx.o ps.o acx.o \
			  boot.o init.o debugfs.o scan.o version.o

wl12xx_compat_spi-objs 	= spi.o
wl12xx_compat_sdio-objs	= sdio.o
wl12xx_compat_sdio_test-objs	= sdio_test.o

wl12xx_compat-$(CONFIG_NL80211_TESTMODE)	+= testmode.o
obj-$(CONFIG_WL12XX_COMPAT)			+= wl12xx_compat.o
obj-$(CONFIG_WL12XX_COMPAT_SPI)		+= wl12xx_compat_spi.o
obj-$(CONFIG_WL12XX_COMPAT_SDIO)	+= wl12xx_compat_sdio.o

obj-$(CONFIG_WL12XX_COMPAT_SDIO_TEST)	+= wl12xx_compat_sdio_test.o

# small builtin driver bit
obj-$(CONFIG_WL12XX_COMPAT_PLATFORM_DATA)	+= wl12xx_platform_data.o
