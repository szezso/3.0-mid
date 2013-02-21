#define INFO_VER 0x10

enum chip_ids {
	PMIC_POWER = 0x2000,
	PMIC_GPIO,
	PMIC_LED_LCD,
	PMIC_RTC,
	PMIC_BATT,

	MSIC_ID,
	MSIC_IRQ,
	MSIC_GPIO,
	MSIC_SVID,
	MSIC_VREG,
	MSIC_RESET,
	MSIC_BURST,
	MSIC_RTC,
	MSIC_CHARGER,
	MSIC_ADC,
	MSIC_AUDIO,
	MSIC_HDMI,

	CHIP_ID_END
};

struct reg_info {
	unsigned short	version;
	unsigned short	valid;
	unsigned int	chip_id;
	unsigned int	register_id;
	unsigned int	register_value;
};
