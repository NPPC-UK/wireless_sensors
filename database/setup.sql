CREATE SCHEMA IF NOT EXISTS `environment_data`;
USE `environment_data` ;

-- -----------------------------------------------------
-- Table `environment_data`.`sensors`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `environment_data`.`sensors` (
  `sensor_id` INT NOT NULL AUTO_INCREMENT,
  `manufacturer` VARCHAR(255) NOT NULL,
  `calibrated` ENUM('YES','NO') NOT NULL,
  `sensor_type` VARCHAR(45) NOT NULL,
  `measurement_unit` VARCHAR(45) NOT NULL,
  `date_purchased` DATE NULL DEFAULT NULL,
  `serial_number` VARCHAR(45) NULL DEFAULT NULL,
  `notes` VARCHAR(255) NULL DEFAULT NULL,
  PRIMARY KEY (`sensor_id`),
  UNIQUE INDEX `sensor_id_UNIQUE` (`sensor_id` ASC))
ENGINE = InnoDB;


-- -----------------------------------------------------
-- Table `environment_data`.`readings`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `environment_data`.`readings` (
  `reading_sensor_id` INT NOT NULL,
  `reading` DECIMAL(10,4) NOT NULL,
  `date_time` DATETIME NOT NULL,
  `voltage` DECIMAL(10,4) NULL,
  PRIMARY KEY (`reading_sensor_id`, `date_time`, `reading`),
  CONSTRAINT `reading_sensor_id`
    FOREIGN KEY (`reading_sensor_id`)
    REFERENCES `environment_data`.`sensors` (`sensor_id`)
    ON DELETE RESTRICT
    ON UPDATE CASCADE)
ENGINE = InnoDB;


-- -----------------------------------------------------
-- Table `environment_data`.`calibration_data`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `environment_data`.`calibration_data` (
  `equation_sensor_id` INT NOT NULL,
  `calibration_date` DATE NOT NULL,
  `equation` VARCHAR(45) NOT NULL,
  `calibration_instrument` VARCHAR(255) NOT NULL,
  `who_calibrated` VARCHAR(45) NOT NULL,
  PRIMARY KEY (`equation_sensor_id`, `calibration_date`),
  CONSTRAINT `equation_sensor_id`
    FOREIGN KEY (`equation_sensor_id`)
    REFERENCES `environment_data`.`sensors` (`sensor_id`)
    ON DELETE RESTRICT
    ON UPDATE CASCADE)
ENGINE = InnoDB;


-- -----------------------------------------------------
-- Table `environment_data`.`locations`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `environment_data`.`locations` (
  `location_sensor_id` INT NOT NULL,
  `x_location` INT(2) NOT NULL,
  `y_location` INT(2) NOT NULL,
  `z_location` INT(2) NOT NULL,
  `compartment` INT(2) NOT NULL,
  `node_id` INT(2) NULL,
  `node_order` INT(2) NULL,
  `network` INT(2) NULL,
  `location_notes` VARCHAR(255) NULL,
  PRIMARY KEY (`location_sensor_id`),
  CONSTRAINT `location_sensor_id`
    FOREIGN KEY (`location_sensor_id`)
    REFERENCES `environment_data`.`sensors` (`sensor_id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


SET SQL_MODE=@OLD_SQL_MODE;
SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;
SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS;
