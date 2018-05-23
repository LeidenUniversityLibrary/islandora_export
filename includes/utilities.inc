<?php
/**
 * @file
 * utilities.inc.
 *
 *
 *  Copyright 2017 Leiden University Library
 *
 *  This file is part of islandora_export.
 *
 *  islandora_export is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

function islandora_export_process_children($obj, $filter, $formatconfig, $directory, &$doneids, $settings = array()) {
  if (!$obj) {
    return;
  }
  if (in_array('islandora:collectionCModel', $obj->models)) {
    module_load_include('inc', 'islandora', 'includes/utilities');
    $pagenumber = isset($settings['pagenumber'])?$settings['pagenumber']:0;
    $pagesize = isset($settings['pagesize'])?$settings['pagesize']:100;
    while (TRUE) {
      list($count, $colobjs) = islandora_basic_collection_get_member_objects($obj, $pagenumber, $pagesize);
      $pagenumber++;
      if (count($colobjs) === 0) {
        break;
      }
      foreach ($colobjs as $colobj) {
        $cid = $colobj['object']['value'];
        if (isset($doneids[$cid])) {
          continue;
        }
        $cobj = islandora_object_load($cid);
        if ($cobj) {
          if (islandora_export_filter_should_export($cobj, $filter)) {
            islandora_export_object_export($cobj, $formatconfig, $directory);
          }
          $doneids[$cid] = 1;
          islandora_export_process_children($cobj, $filter, $formatconfig, $directory, $doneids);
        }
      }
    }
  }
  if (in_array('islandora:bookCModel', $obj->models)) {
    module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
    $pages = islandora_paged_content_get_pages($obj);
    foreach ($pages as $pagepid => $pageinfo) {
      if (isset($doneids[$pagepid])) {
        continue;
      }
      $pageobj = islandora_object_load($pagepid);
      if ($pageobj) {
        if (islandora_export_filter_should_export($pageobj, $filter)) {
          islandora_export_object_export($pageobj, $formatconfig, $directory);
        }
        $doneids[$pagepid] = 1;
      }
    }
  }
  if (in_array('islandora:compoundCModel', $obj->models)) {
    $compoundparts = islandora_compound_object_get_parts($obj->id);
    foreach ($compoundparts as $partpid) {
      if (isset($doneids[$partpid])) {
        continue;
      }
      $pobj = islandora_object_load($partpid);
      if ($pobj) {
        if (islandora_export_filter_should_export($pobj, $filter)) {
          islandora_export_object_export($pobj, $formatconfig, $directory);
        }
        $doneids[$partpid] = 1;
        islandora_export_process_children($pobj, $filter, $formatconfig, $directory, $doneids);
      }
    }
  }
}

function islandora_export_filter_should_export($object, $filter) {
  if (isset($filter['excluded_cmodels'])) {
    if (array_intersect($filter['excluded_cmodels'], $object->models)) {
      return FALSE;
    }
  }
  if (isset($filter['included_cmodels'])) {
    if (!array_intersect($filter['included_cmodels'], $object->models)) {
      return FALSE;
    }
  }
  return TRUE;
}

function islandora_export_object_export($obj, array $formatconfig, $outputdirectory) {
  if ($formatconfig['exportformat']['type'] === 'CSV') {
    $csvfile = rtrim($outputdirectory, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . 'export.csv';
    $fileexistsalready = is_file($csvfile);
    if ($fileexistsalready) {
      $csvfilehandle = fopen($csvfile, 'a');
    }
    else {
      $csvfilehandle = fopen($csvfile, 'w');
    }
    if ($csvfilehandle === FALSE) {
      return drush_set_error("File $csvfile cannot be opened for writing"); 
    } 
    if (!$fileexistsalready) {
      $separator = $formatconfig['exportformat']['separator'];
      $columns = $formatconfig['exportformat']['columns']; 
      if (fputcsv($csvfilehandle, $columns, $separator) === FALSE) {
        return drush_set_error("Failed to write to file $csvfile");
      } 
    }
    $ok = islandora_export_object_export_csv($obj, $formatconfig, $outputdirectory, $csvfilehandle);
    fclose($csvfilehandle);
    if ($ok === FALSE) {
      return FALSE;
    }
  }
  drush_log(t("Exported object @id succesfully", array('@id' => $obj->id)), 'ok');
}

function islandora_export_object_export_csv($obj, array $formatconfig, $outputdirectory, $csvfilehandle) {
  $separator = $formatconfig['exportformat']['separator'];
  $columnnames = $formatconfig['exportformat']['columns'];

  $columnvalues = array();
  foreach ($columnnames as $columnname) {
    unset($columnconfig);
    if (isset($formatconfig['exportformat']['columntypes'])) {
      $columntypes = $formatconfig['exportformat']['columntypes'];
      $cmodels = $obj->models; 
      foreach ($columntypes as $type => $cmodel) {
        if (in_array($cmodel, $cmodels)) {
          if (isset($formatconfig[$type . ':' . $columnname])) {
            $columnconfig = $formatconfig[$type . ':' . $columnname];
          }
        }
      }
    }
    if (!isset($columnconfig) && isset($formatconfig[$columnname])) {
      $columnconfig = $formatconfig[$columnname];
    }
    if (!isset($columnconfig)) {
      return drush_set_error("No config found for column '$columnname'. Please use a general config or a specific for every type.");
    }
    $value = FALSE;
    if ($columnconfig['type'] === 'string') {
      $value = $columnconfig['string'];
    }
    elseif ($columnconfig['type'] === 'value') { 
      $value = islandora_export_object_export_value($obj, $columnconfig);
    }
    elseif ($columnconfig['type'] === 'file') {
      $filename = islandora_export_object_export_file($obj, $formatconfig, $columnconfig, $outputdirectory);
      if ($filename) {
        $value = $filename;
      }
    }
    if ($value === FALSE) {
      $value = '';
    }
    $columnvalues[] = $value;
  }
  if (fputcsv($csvfilehandle, $columnvalues, $separator) === FALSE) {
    return drush_set_error("Failed to write to csv file");
  }
  return TRUE;
}

function islandora_export_object_export_value($obj, array $columnconfig) {
  $value = FALSE;
  $source = $columnconfig['source'];
  if ($source['type'] === 'property') {
    $value = islandora_export_object_property($obj, $source['property']);
  }
  elseif ($source['type'] === 'datastream') {
    $dsid = $source['dsid'];
    $ds = $obj[$dsid];
    if ($ds) {
      $extract = $columnconfig['extract'];
      if ($extract['type'] === 'property') {
        switch ($extract['property']) {
          case 'checksum':
            $value = $ds->checksum;
            break;
          case 'checksumtype':
            $value = $ds->checksumType;
            break;
          case 'controlgroup':
            $value = $ds->controlGroup;
            break;
          case 'creationdate':
	    $value = $ds->createdDate->format("Y-m-d");
            break;
	  case 'creationdatetime':
	    $value = $ds->createdDate->format("Y-m-d\TH:i:s");
	    break;
          case 'id':
            $value = $ds->id;
            break;
          case 'label':
            $value = $ds->label;
            break;
          case 'mimetype':
            $value = $ds->mimetype;
            break;
          case 'size':
            $value = $ds->size;
            break;
          case 'state':
            $value = $ds->state;
            break;
          case 'url':
            $value = $ds->url;
            break;
          case 'extension':
            module_load_include('inc', 'islandora', 'includes/mimetype.utils');
            $value = islandora_get_extension_for_mimetype($ds->mimetype);
            break;
        }
      }
      elseif ($extract['type'] === 'xpath') {
        $xpath = $extract['xpath'];
	$content = $ds->content;
	$domdoc = new DOMDocument();
	if ($domdoc->loadXML($content)) {
	  $domxpath = new DOMXPath($domdoc);
          $namespaces = explode(';', $extract['namespaces']);
          foreach ($namespaces as $namespace) {
            list($prefix, $uri) = explode(' ', $namespace, 2);
	    $domxpath->registerNamespace($prefix, $uri);
          }
	  $domnodelist = $domxpath->query($xpath);
	  if ($domnodelist->length > 0) {
	    foreach ($domnodelist as $domnode) {
	      $text = $domnode->textContent;
	      if (isset($text) && strlen($text) > 0) {
		$value = $text;
		break;
	      }
	    }
	  }
	}
      }
    }
  }
  return $value;
}

function islandora_export_object_property($obj, $property) {
  $value = FALSE;
  switch ($property) {
    case 'creationdate':
      $value = $obj->createdDate->format("Y-m-d");
      break;
    case 'creationdatetime':
      $value = $obj->createdDate->format("Y-m-d\TH:i:s");
      break;
    case 'id':
      $value = $obj->id;
      break;
    case 'label':
      $value = $obj->label;
      break;
    case 'modifydate':
      $value = $obj->lastModifiedDate->format("Y-m-d");
      break;
    case 'modifydatetime':
      $value = $obj->lastModifiedDate->format("Y-m-d\TH:i:s");
      break;
    case 'owner':
      $value = $obj->owner;
      break;
    case 'state':
      $value = $obj->state;
      break;
    case 'cmodels':
      $value = implode(',', array_diff($obj->models, array('fedora-system:FedoraObject-3.0')));
      break;
    case 'parents':
      if (module_load_include('inc', 'islandora_basic_collection', 'includes/utilities')) {
        $value = implode(',', islandora_basic_collection_get_parent_pids($obj));
      }
      break;
  }
  return $value;
}

function islandora_export_object_export_file($obj, array $formatconfig, array $columnconfig, $outputdirectory) {
  $filepath = '';
  $outputdir = rtrim($outputdirectory, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR;
  if (isset($columnconfig['outputdirectory'])) {
    $dir = islandora_export_object_export_filepath_part($obj, $formatconfig, $columnconfig['outputdirectory']);
    if ($dir) {
      $filepath = $filepath . rtrim($dir, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR;
    }
  }
  if (preg_match('#[^a-zA-Z0-9./_-]#', $filepath)) {
    return drush_set_error(t('Filepath @filepath contains illegal characters', array('@filepath' => $filepath)));
  }
  if (!is_dir($outputdir . $filepath)) {
    mkdir($outputdir . $filepath, 0777, TRUE); 
  }
  $filename = islandora_export_object_export_filepath_part($obj, $formatconfig, $columnconfig['outputfilename']);
  if (!$filename) {
    return FALSE;
  }
  if (preg_match('#[^a-zA-Z0-9._-]#', $filename)) {
    return drush_set_error(t('Filename @filename contains illegal characters', array('@filename' => $filename)));
  }
  $filepath .= $filename;
  if (isset($columnconfig['outputextension'])) {
    $extension = islandora_export_object_export_filepath_part($obj, $formatconfig, $columnconfig['outputextension']);
    if ($extension) {
      if (preg_match('#[^a-zA-Z0-9._-]#', $extension)) {
	return drush_set_error(t('Extension @extension contains illegal characters', array('@extension' => $extension)));
      }
      $filepath .= '.' . ltrim($extension, '.'); 
    }
  }
  
  $source = $columnconfig['source'];
  $fileexists = is_file($outputdir . $filepath);
  if ($source['type'] === 'property') {
    $value = islandora_export_object_property($obj, $source['property']);
    if ($value) {
      file_put_contents($outputdir . $filepath, $value);
    }
  }
  elseif ($source['type'] === 'datastream') {
    $dsid = $source['dsid'];
    $ds = $obj[$dsid];
    if ($ds) {
      $ds->getContent($outputdir . $filepath);
    }
    else {
      $filepath = FALSE; // no datastream to write
    }
  }
  if ($filepath !== FALSE && $fileexists) {
    drush_log(t("File '@filepath' is overwritten because it existed already.", array("@filepath" => $filepath)), "warning");
  }
  return $filepath;
}

function islandora_export_object_export_filepath_part($obj, array $formatconfig, array $outputconfig) {
  $part = FALSE;
  if (isset($outputconfig['string'])) {
    $part = $outputconfig['string'];
  }
  elseif (isset($outputconfig['like'])) {
    $columnname = $outputconfig['like'];
    if (isset($formatconfig[$columnname])) {
      $part = islandora_export_object_export_value($obj, $formatconfig[$columnname]); 
      if ($part) {
	$part = preg_replace('#[^a-zA-Z0-9._-]+#', '_', $part);
      }
    }
  }
  return $part; 
}

function islandora_export_validate_ini_column($formatfile, array $formatconfig, $column) {
  if (!isset($formatconfig[$column])) {
    return drush_set_error("format_file '$formatfile' does not have a definition for column '$column'");
  }
  if (!isset($formatconfig[$column]['type'])) {
    return drush_set_error("format_file '$formatfile' section [$column]: type should exist");
  }
  $type = $formatconfig[$column]['type'];
  if ($type !== 'string' && $type !== 'value' && $type !== 'file') {
    return drush_set_error("format_file '$formatfile' section [$column]: type should be 'string', 'value' or 'file'");
  }
  if ($type === 'string') {
    if (!isset($formatconfig[$column]['string'])) {
      return drush_set_error("format_file '$formatfile' section [$column]: string should exist.");
    }
  }
  else {
    if (!isset($formatconfig[$column]['source']['type'])) {
      return drush_set_error("format_file '$formatfile' section [$column]: source[type] should exist");
    }
    $sourcetype = $formatconfig[$column]['source']['type'];
    if ($sourcetype !== 'property' && $sourcetype !== 'datastream') {
      return drush_set_error("format_file '$formatfile' section [$column]: source[type] should be either 'property' or 'datastream'");
    }
    if ($sourcetype === 'property') {
      $property = $formatconfig[$column]['source']['property'];
      if (!isset($property)) {
	return drush_set_error("format_file '$formatfile' section [$column]: source[property] should exist");
      }
      $possibleproperties = array('checksum', 'checksumtype', 'creationdate', 'creationdatetime', 'modifydate', 'modifydatetime', 'id', 'label', 'owner', 'mimetype', 'size', 'state', 'url', 'extension', 'cmodels', 'parents');
      if (!in_array($property, $possibleproperties)) {
	return drush_set_error(t("format_file '$formatfile' section [$column]: source[property] should be one of these: @props",
        array('@props' => implode(',', $possibleproperties))));
      }
    }
    elseif ($sourcetype === 'datastream') {
      $dsid = $formatconfig[$column]['source']['dsid'];
      if (!isset($dsid)) {
	return drush_set_error("format_file '$formatfile' section [$column]: source[dsid] should exist");
      }
    }
    if ($type === 'value') {
      if (isset($formatconfig[$column]['extract'])) {
	$extracttype = $formatconfig[$column]['extract']['type'];
	if (!isset($extracttype) || ($extracttype !== 'xpath' && $extracttype !== 'property')) {
	  return drush_set_error("format_file '$formatfile' section [$column]: extract[type] should have value 'xpath' or 'property'");
	}
	if (!isset($formatconfig[$column]['extract'][$extracttype])) {
	  return drush_set_error("format_file '$formatfile' section [$column]: extract[$extracttype] should exist");
	}
	if (isset($formatconfig[$column]['extract']['xpath'])) {
	  $xpath = $formatconfig[$column]['extract']['xpath'];
	  $doc = new DOMDocument;
	  $domxpath = new DOMXPath($doc);
	  if (isset($formatconfig[$column]['extract']['namespaces'])) {
	    $namespaces = explode(';', $formatconfig[$column]['extract']['namespaces']);
	    foreach ($namespaces as $namespace) {
	      $parts = explode(' ', $namespace);
	      if (count($parts) != 2) {
		return drush_set_error("format_file '$formatfile' section [$column]: extract[namespaces]: wrong format '$namespace', should be 'prefix uri'. Multiple namespaces separated by semicolon ;.");
	      }
	      list($prefix, $uri) = $parts;
	      if (!(filter_var($uri, FILTER_VALIDATE_URL) || (strpos($uri, 'urn:') === 0))) {
		drupal_set_message("format_file '$formatfile' section [$column]: extract[namespaces]: not a URI in '$namespace'", 'warning');
	      }
	      if (@$domxpath->registerNamespace($prefix, $uri) === FALSE) {
		return drush_set_error("format_file '$formatfile' section [$column]: extract[namespaces]: '$prefix=$uri' is not valid");
	      }
	    }
	  }
	  if (@$domxpath->query($xpath) === FALSE) {
	    return drush_set_error("format_file '$formatfile' section [$column]: extract[xpath]: '$xpath' is not valid");
	  }
	}
      } 
    }
    elseif ($type === 'file') {
      if (!islandora_export_validate_ini_column_output($formatfile, $formatconfig, $column, 'directory')) {
	return FALSE;
      }
      if (!islandora_export_validate_ini_column_output($formatfile, $formatconfig, $column, 'filename')) {
	return FALSE;
      }
      if (!islandora_export_validate_ini_column_output($formatfile, $formatconfig, $column, 'extension')) {
	return FALSE;
      }
    }
  }
  return TRUE;
}

function islandora_export_validate_ini_column_output($formatfile, array $formatconfig, $column, $type) {
  if (!isset($formatconfig[$column]['output' . $type])) {
    if ($type === 'filename') {
      return drush_set_error("format_file '$formatfile' section [$column]: output$type should exist");
    }
    else {
      return TRUE;
    }
  }
  if (count($formatconfig[$column]['output' . $type]) != 1 || !(isset($formatconfig[$column]['output' . $type]['string']) || isset($formatconfig[$column]['output' . $type]['like']))) {
    return drush_set_error("format_file '$formatfile' section [$column]: either output${type}[string] or output${type}[like] should exist");
  }
  if (isset($formatconfig[$column]['output' . $type]['like'])) {
    $likecol = $formatconfig[$column]['output' . $type]['like'];
    if ($likecol === $column) {
      return drush_set_error("format_file '$formatfile' section [$column]: output${type}[like] refers to itself");
    }
    if (!isset($formatconfig[$likecol])) { 
      return drush_set_error("format_file '$formatfile' does not have a definition for column '$likecol' referred to by section [$column] output${type}[like]");
    } 
    if (!(isset($formatconfig[$likecol]['type']) && $formatconfig[$likecol]['type'] === 'value')) {
      return drush_set_error("format_file '$formatfile' section [$column]: output${type}[like] should refer to a section of type 'value'");
    }
    if (!islandora_export_validate_ini_column($formatfile, $formatconfig, $likecol)) {
      return FALSE;
    }
  }
  return TRUE;
}

function islandora_export_ids_from_file($idsfile) {
  $foundids = array();

  $idsstr = file_get_contents($idsfile);
  $fieldseparator = "";
  $ids = str_getcsv($idsstr, "\n");
  foreach ($ids as $index => $id) {
    if (strlen($id) == 0) {
      continue;
    }
    if ($fieldseparator === "") {
      $fieldseparator = "\t";
      $comps = str_getcsv($id, $fieldseparator);
      if (count($comps) > 1) {
        // Fields are separated by a tab.
      }
      else {
        $fieldseparator = ",";
        $comps = str_getcsv($id, $fieldseparator);
        if (count($comps) > 1) {
          // Fields are separated by a comma.
        }
        else {
          $fieldseparator = ";";
          $comps = str_getcsv($id, $fieldseparator);
          if (count($comps) > 1) {
            // Fields are separated by a semicolon.
          }
          else {
            // Fields are not separated.
            $fieldseparator = FALSE;
          }
        }
      }
    }
    if ($fieldseparator !== FALSE) {
      $comps = str_getcsv($id, $fieldseparator);
      if (count($comps) > 1) {
        $id = $comps[0];
      }
    }
    $foundids[] = $id;
  }
  return $foundids;
}
