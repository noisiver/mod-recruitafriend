DROP PROCEDURE IF EXISTS `AddColumn`;

DELIMITER $$

CREATE PROCEDURE `AddColumn` (
    `@TABLE` VARCHAR(100),
    `@COLUMN` VARCHAR(100),
    `@TYPE` VARCHAR(100),
    `@DEFAULT` VARCHAR(100),
    `@AFTER` VARCHAR(100))

`AddColumn` : BEGIN
    DECLARE `@EXISTS` INT UNSIGNED DEFAULT 0;

    SELECT COUNT(*) INTO `@EXISTS` 
    FROM `information_schema`.`COLUMNS` WHERE 
    `TABLE_SCHEMA` = DATABASE() AND 
    `TABLE_NAME` = `@TABLE` AND 
    `COLUMN_NAME` = `@COLUMN`;

    IF (`@EXISTS` < 1) THEN
        SET @SQL = CONCAT(
            'ALTER TABLE `', 
            `@TABLE`, 
            '` ADD COLUMN `',
            `@COLUMN`,
            '` ',
            `@TYPE`, 
            ' NOT NULL DEFAULT ',
            `@DEFAULT`,
            ' AFTER `',
            `@AFTER`,
            '`;'
        );

        PREPARE query FROM @SQL;
        EXECUTE query;
    END IF;
END $$

DELIMITER ;

CALL AddColumn('characters', 'referRewarded', 'TINYINT', '0', 'grantableLevels');

DROP PROCEDURE IF EXISTS `AddColumn`;
