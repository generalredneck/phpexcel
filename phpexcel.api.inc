<?php

/**
 * Simple API function which will generate an XLS file and
 * save it in $path.
 *
 * @param array $headers
 * 						An array containing all headers. If given a two-dimensional array, each first dimension
 * 						entry will be on a separate worksheet ($headers[sheet][header]).
 * @param array $data
 * 						A two-dimensional array containing all data ($data[row][column]). If given a three-dimensional array, each first dimension
 * 						entry will be on a separate worksheet ($data[sheet][row][column]).
 * @param string $path
 * 						The path where the file must be saved. Must be writable.
 * @param array $options
 * 						An array which allows to set some specific options.
 * 						Allowed keys:
 * 								[format] = The EXCEL format. Can be either 'xls' or 'xlsx'
 * 								[creator] = The name of the creator
 * 								[title] = The title
 * 								[subject] = The subject
 * 								[description] = The description
 *
 * @return bool
 * 						TRUE on success, FALSE on error. Look in watchdog logs for information
 *            about errors.
 */
function phpexcel_export($headers = array(), $data = array(), $path = '', $options = NULL) {
  if (!count($headers)) {
    watchdog('phpexcel', "You must provide at lease one header entry!", array(), WATCHDOG_ERROR);
		
    return FALSE;
  }
  
  if (!count($data)) {
    watchdog('phpexcel', "No data provided!", array(), WATCHDOG_ERROR);
		
    return FALSE;
  }
  
	// Must assign to a variable, as file_check_path() will alter the passed argument
	$check_directory = $path;
	
  if (!file_check_path($check_directory)) {
    watchdog('phpexcel', "Path '@path' is not writable!", array('@path' => $path), WATCHDOG_ERROR);
		
    return FALSE;
  }
  
	if (!file_check_location('sites/all/libraries/PHPExcel/PHPExcel.php', 'sites/all/libraries/PHPExcel')) {
		watchdog('phpexcel', "Couldn't find the PHPExcel library! ", array(), WATCHDOG_ERROR);
		
		return FALSE;	
	}
	
  require_once 'sites/all/libraries/PHPExcel/PHPExcel.php';
	
	$path = file_munge_filename($path, 'xls xlsx');
  
  if (!$cache_method = PHPExcel_CachedObjectStorageFactory::cache_to_memcache) {
		$cache_method = PHPExcel_CachedObjectStorageFactory::cache_in_memory_gzip;
		$cache_settings = array();
	}
	else {
		$cache_settings = array('memcacheServer'  => 'localhost');
	}
  
  PHPExcel_Settings::setCacheStorageMethod($cache_method, $cache_settings);
	
	$xls = new PHPExcel();
  
  _phpexcel_set_properties($xls->getProperties(), $options);
  _phpexcel_set_headers($xls, $headers);
  _phpexcel_set_columns($xls, $data, count($headers[0]));
  
  if (!isset($options['format']) || $options['format'] == 'xls') {
		$writer = new PHPExcel_Writer_Excel5($xls);
		$writer->save($path);
	}
	else {
		$writer = new PHPExcel_Writer_Excel2007($xls);
		$writer->save($path);
	}
	
	return file_exists($path);
}

/**
 * Simple API function which allows to export a db_query() result to an Excel file.
 * The headers will be set to the names of the exported columns.
 *
 * @see phpexcel_export()
 *
 * @param result $result
 * 							The MySQL result object.
 * @param string $path
 * 							The path where the file should be saved. Must be writable.
 * @param array $options
 * 							An array which allows to set some specific options.
 *
 * @return bool
 * 							TRUE on success, FALSE on error. Look into watchdog logs for information
 *              about errors.
 */
function phpexcel_export_db_result($result, $path, $options = array()) {
	$data = array();
	
	while ($row = db_fetch_array($result)) {
		if (!$headers) {
			$headers = array_keys($row);
		}
		$data[] = array_values($row);
	}
	
	return phpexcel_export($headers, $data, $path, $options);
}

/**
 * Sets the Excel file properties, like creator, title, etc.
 *
 * @see phpexcel_export()
 */
function _phpexcel_set_properties(&$properties, $options) {
  if ($options['creator']) {
    $properties->setCreator($options['creator']);
  }
  else {
    $properties->setCreator("PHPExcel");
  }
  
  if ($options['title']) {
		$properties->setTitle($options['title']);
  }
  
  if ($options['subject']) {
		$properties->setSubject($options['subject']);
  }
  
  if ($options['description']) {
		$properties->setSubject($options['description']);
  }
}

/**
 * Sets the Excel file headers.
 *
 * @see phpexcel_export()
 */
function _phpexcel_set_headers(&$xls, &$headers) {
	$headers = array_values($headers);
	
	if (!is_array($headers[0])) {
		$headers = array($headers);
	}
	
	foreach ($headers as $sheet_id => $sheet_headers) {
		$sheet = $xls->setActiveSheetIndex($sheet_id);
  
		for ($i = 0, $len = count($sheet_headers); $i < $len; $i++) {
			$sheet->setCellValueByColumnAndRow($i, 1, trim($sheet_headers[$i]));
		}
	}
}

/**
 * Adds the data to the Excel file.
 *
 * @see phpexcel_export()
 */
function _phpexcel_set_columns(&$xls, &$data, $cols) {
	$data = array_values($data);
	
	if (!is_array($data[0][0])) {
		$data = array($data);
	}
	
	foreach ($data as $sheet_id => $sheet_data) {
		$sheet = $xls->setActiveSheetIndex($sheet_id);
	
		for ($i = 0, $len = count($sheet_data); $i < $len; $i++) {
			for ($j = 0; $j < $cols; $j++) {
				// We must offset the row count (by 2, because PHPExcel starts the count at 1), because the first row is used by the headers
				$sheet->setCellValueByColumnAndRow($j, $i + 2, $sheet_data[$i][$j]);
			}
		}
	}
}

/**
 * Simple API function that will load an Excel file from $path and parse it
 * as a multidimensional array.
 *
 * @param string $path
 * 						The path to the Excel file. Must be readable.
 * @param boolean $keyed_by_headers = TRUE
 * 						If TRUE, will key the row array with the header values and will
 * 						skip the header row. If FALSE, will contain the headers in the first
 * 						row and the rows will be keyed numerically.
 * @return array|boolean
 * 						The parsed data as an array on success, FALSE on error. Look into
 * 						watchdog logs for information about errors.
 */
function phpexcel_import($path, $keyed_by_headers = TRUE) {
	if (is_readable($path)) {
		require_once 'sites/all/libraries/PHPExcel/PHPExcel.php';
		
		$xls_reader = PHPExcel_IOFactory::createReaderForFile($path);
		
		$xls_reader->setReadDataOnly(TRUE);
		
		$xls_data = $xls_reader->load($path);
		
		$data = array();
		$headers = array();
		$i = 0;
		
		foreach ($xls_data->getWorksheetIterator() as $worksheet) {
			$j = 0;
			
			foreach ($worksheet->getRowIterator() as $row) {
				$k = 0;
				
				$cells = $row->getCellIterator();
				
				$cells->setIterateOnlyExistingCells(FALSE);
				
				foreach($cells as $cell) {
					if (!$j && $keyed_by_headers) {
						$headers[] = $cell->getValue() ? $cell->getValue() : $k;
					}
					elseif ($keyed_by_headers) {
						$data[$i][$j - 1][$headers[$k]] = $cell->getValue() ? $cell->getValue() : '';
					}
					else {
						$data[$i][$j][] = $cell->getValue() ? $cell->getValue() : '';
					}
					
					$k++;
				}
				
				$j++;
			}
			
			$i++;
		}
		
		return $data;
	}
	else {
		watchdog('phpexcel', "The path '!path' is not readable !", array('!path' => $path));
		
		return FALSE;
	}
}