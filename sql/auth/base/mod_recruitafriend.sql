CREATE TABLE IF NOT EXISTS `mod_recruitafriend` (
	`id` INT(10) UNSIGNED NOT NULL,
	`recruiter` INT(10) UNSIGNED NOT NULL,
	`referral_date` TIMESTAMP NOT NULL DEFAULT current_timestamp(),
	`status` TINYINT(3) UNSIGNED NOT NULL DEFAULT '1',
	PRIMARY KEY (`account_id`) USING BTREE
)
COLLATE='utf8mb4_general_ci'
ENGINE=InnoDB
;
