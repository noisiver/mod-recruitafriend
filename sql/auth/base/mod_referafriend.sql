CREATE TABLE IF NOT EXISTS `mod_referafriend` (
	`id` INT(10) UNSIGNED NOT NULL,
	`referrer` INT(10) UNSIGNED NOT NULL,
	`referral_date` TIMESTAMP NOT NULL DEFAULT current_timestamp(),
	`active` TINYINT(3) UNSIGNED NOT NULL DEFAULT '1',
	PRIMARY KEY (`id`) USING BTREE
)
COLLATE='utf8mb4_general_ci'
ENGINE=InnoDB
;
