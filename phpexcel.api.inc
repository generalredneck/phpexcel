<?php

/**
 * @file
 * Deprecated. All modules should now include phpexcel.inc instead of this file.
 * This will prevent confusion between the phpexcel.api.inc and phpexcel.api.php files
 */

require_once 'phpexcel.inc';

drupal_set_message(t("A module is including the phpexcel/phpexcel.api.inc file. This file is deprecated. Code should be updated to include the new phpexcel/phpexcel.inc file instead."), 'warning');