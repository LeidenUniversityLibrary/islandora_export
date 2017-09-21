<?php
/**
 * @file
 * Functions for exporting Islandora content via drush.
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

/**
 * Implements hook_drush_command().
 */
function islandora_export_drush_command() {
  $items['islandora_export'] = array(
    'description' => 'Exports the metadata and/or datastreams of items from Islandora. You can specify which items to use for the export, like items in specific collection(s), batch set(s) and/or a file with identifiers or a Solr query. Optionally you can specify the type of the source objects you need. Provide an ini file to specify the output format you want.',
    'options' => array(
      'format_file' => 'Mandatory, the absolute filepath to an ini file containing the format of the export. See documentation for specifics.',
      'directory' => 'Mandatory, the absolute filepath to an empty directory to export to.',
      'collection' => 'Optionally, one or more collection IDs, separated by comma\'s.',
      'batchset' => 'Optionally, one or more batchset IDs, separated by comma\'s.',
      'ids_file' => 'Optionally, the absolute filepath to a file containing a list of Islandora identifiers.',
      'solr_query' => 'Optionally, a Solr query to find the items to export. Cannot be combined with collection, batchset or ids_file.',
      'cmodel' => 'Optionally, filters the objects found by collection/batchset/ids_file on the given content models(s), separated by comma\'s.',
    ),
    'aliases' => array('export'),
    'examples' => array(
      'drush --user=admin islandora_export --collection=islandora:root --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --batchset=66 --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --ids_file=/path/to/idsfile.csv --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --solr_query=catch_all_fields_mt:book --cmodel=islandora:bookCModel --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
    ),
  );
  return $items;
}

/**
 * Implements drush_hook_COMMAND_validate().
 */
function drush_islandora_export_islandora_export_validate() {
  module_load_include('inc', 'islandora_export', 'includes/utilities');

  $collections = drush_get_option('collection');
  $batchsets = drush_get_option('batchset');
  $idsfile = drush_get_option('ids_file');
  $solrquery = drush_get_option('solr_query');
  $cmodels = drush_get_option('cmodel');
  $formatfile = drush_get_option('format_file');
  $directory = drush_get_option('directory');

  // check directory option
  if (!isset($directory)) {
    return drush_set_error("Argument directory is mandatory!");
  }
  if (!is_dir($directory)) {
    return drush_set_error("directory '$directory' does not exist, should be an absolute path");
  }
  // Check format_file option.
  if (!isset($formatfile)) {
    return drush_set_error("Argument format_file is mandatory!");
  }
  if (!file_exists($formatfile)) {
    return drush_set_error("format_file '$formatfile' does not exist, should be an absolute path");
  }
  $formatconfig = parse_ini_file($formatfile, TRUE);
  if ($formatconfig === FALSE) {
    return drush_set_error("format_file '$formatfile' is not a valid ini file");
  }
  $formatfilename = basename($formatfile);
  if (!isset($formatconfig['exportformat'])) {
    return drush_set_error("format_file '$formatfilename' does not have the mandatory section [exportformat]");
  }
  if (!(isset($formatconfig['exportformat']['type']) && $formatconfig['exportformat']['type'] === 'CSV')) {
    return drush_set_error("format_file '$formatfilename' section [exportformat]: only type CSV is allowed at present");
  }
  if (!(isset($formatconfig['exportformat']['separator']) && strlen($formatconfig['exportformat']['separator']) == 1)) {
    return drush_set_error("format_file '$formatfilename' section [exportformat]: separator is mandatory and should be a single character.");
  }
  if (!(isset($formatconfig['exportformat']['columns']) && is_array($formatconfig['exportformat']['columns']) && count($formatconfig['exportformat']['columns']) > 0)) {
    return drush_set_error("format_file '$formatfilename' section [exportformat]: one or more columns[] are required");
  }
  $columns = $formatconfig['exportformat']['columns'];
  if (isset($formatconfig['exportformat']['columntypes'])) {
    $columntypes = $formatconfig['exportformat']['columntypes'];
    foreach ($columns as $column) {
      $hasdef = FALSE;
      if (isset($formatconfig[$column])) {
        $hasdef = TRUE;
        if (!islandora_export_validate_ini_column($formatfilename, $formatconfig, $column)) {
          return FALSE;
        }
      }
      foreach ($columntypes as $type => $cmodel) {
        $specificcolumn = $type . ':' . $column;
        if (isset($formatconfig[$specificcolumn])) {
          $hasdef = TRUE;
	  if (!islandora_export_validate_ini_column($formatfilename, $formatconfig, $specificcolumn)) {
	    return FALSE;
	  }
        }
      }
    }
  }
  else {
    foreach ($columns as $column) {
      if (!islandora_export_validate_ini_column($formatfilename, $formatconfig, $column)) {
	return FALSE;
      }
    }
  }

  // Check collection option.
  if (isset($collections)) {
    $colarray = explode(',', $collections);
    foreach ($colarray as $collection) {
      $collectionobj = islandora_object_load($collection);
      if (!($collectionobj && in_array('islandora:collectionCModel', $collectionobj->models))) {
	return drush_set_error("Identifier '$collection' does not resolve to a collection");
      }
    }
  }
 
  // Check batchset option.
  if (isset($batchsets)) {
    if (module_load_include('inc', 'islandora_batch', 'includes/db')) {
      $batchsetarray = explode(',', $batchsets);
      foreach ($batchsetarray as $batchset) {
        $itemids = islandora_batch_get_queued_items_for_set($batchset);
        if (!($itemids && count($itemids) > 0)) {
	  return drush_set_error("Identifier '$batchset' does not resolve to a batch set");
        }
      }
    }
    else {
      return drush_set_error("Required module 'islandora_batch' failed to load.");
    }
  }

  // Check ids_file option.
  if (isset($idsfile)) {
    if (!file_exists($idsfile)) {
      return drush_set_error("ids_file '$idsfile' does not exist");
    }
  }

  // Check solr_query option.
  if (isset($solrquery)) {
    if (isset($idsfile) || isset($batchsets) || isset($collections)) {
      return drush_set_error("solr_query cannot be combined with collection, batchset or ids_file.");
    } 
    $parts = explode(':', $solrquery);
    if (count($parts) != 2) {
      return drush_set_error("solr_query should have the following format: solr_field:search_term.");
    }
  }

  return TRUE;
}

/**
 * Implements drush_hook_COMMAND().
 */
function drush_islandora_export_islandora_export() {
  module_load_include('inc', 'islandora_export', 'includes/utilities');

  $collections = drush_get_option('collection');
  $batchsets = drush_get_option('batchset');
  $idsfile = drush_get_option('ids_file');
  $solrquery = drush_get_option('solr_query');
  $cmodels = drush_get_option('cmodel');
  $formatfile = drush_get_option('format_file');
  $directory = drush_get_option('directory');

  if (isset($cmodels)) {
    $cmodelarray = explode(',', $cmodels);
  }

  $doneids = array();
  $formatconfig = parse_ini_file($formatfile, TRUE);
  if (isset($collections)) {
    $colarray = explode(',', $collections);
    foreach ($colarray as $collection) {
      $collectionobj = islandora_object_load($collection);
      if ($collectionobj && in_array('islandora:collectionCModel', $collectionobj->models)) {
        islandora_export_process_children($collectionobj, $cmodels, $formatconfig, $directory, $doneids);
      }
    }
  }
  if (isset($batchsets)) {
    $batchsetarray = explode(',', $batchsets);
    foreach ($batchsetarray as $batchset) {
      $itemids = islandora_batch_get_queued_items_for_set($batchset);
      foreach ($itemids as $itemid) {
	$obj = islandora_object_load($itemid);
	if ($obj) {
	  if (isset($cmodelarray)) { 
	    if (!array_intersect($cmodelarray, $obj->models)) { 
	      continue;
	    } 
	  } 
	  islandora_export_object_export($obj, $formatconfig, $directory);
          $doneids[$itemid] = 1;
        }
      }
    }
  }
  if (isset($idsfile)) {
    $ids = islandora_export_ids_from_file($idsfile);
    foreach ($ids as $itemid) {
      $obj = islandora_object_load($itemid);
      if ($obj) {
        if (isset($cmodelarray)) { 
         if (!array_intersect($cmodelarray, $obj->models)) { 
           continue;
         } 
        } 
        islandora_export_object_export($obj, $formatconfig, $directory);
        $doneids[$itemid] = 1;
        islandora_export_process_children($obj, $cmodels, $formatconfig, $directory, $doneids);
      }
    }
  }
  if (isset($solrquery)) {
    $current = 0;
    $numfound = 1;
    while ($current < $numfound) {
      $qp = new IslandoraSolrQueryProcessor();
      $qp->buildQuery($solrquery);
      $qp->solrLimit = 50;
      $qp->solrStart = $current;
      $qp->executeQuery();
      $r = $qp->islandoraSolrResult;
      $numfound = $r['response']['numFound'];
      if ($current === 0) {
        drush_log(t("Found @numfound objects for query @query", array("@numfound" => $numfound, "@query" => $solrquery)), 'ok');
      }

      $len = count($r['response']['objects']);
      for ($i = 0; $i < $len; $i++) {
        $objid = $r['response']['objects'][$i]['PID']; 
        $obj = islandora_object_load($objid);
        if ($obj) {
          if (isset($cmodelarray)) {
           if (!array_intersect($cmodelarray, $obj->models)) {
             continue;
           }
          }
          islandora_export_object_export($obj, $formatconfig, $directory);
          $doneids[$objid] = 1;
          islandora_export_process_children($obj, $cmodels, $formatconfig, $directory, $doneids);
        }
      }
      $current += $len;
    }
  }
}

