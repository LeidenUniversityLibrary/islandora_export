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
      'ids_file' => 'Optionally, the absolute filepath to a file containing a list of Islandora identifiers that will be exported.',
      'ids_solr_key' => 'Optionally, the Solr key to use for searching the IDs of the ids_file. Can only be used with ids_file option.',
      'solr_query' => 'Optionally, a Solr query to find the items to export. Cannot be combined with collection, batchset or ids_file.',
      'date_range' => 'Optionally, exports the items from Islandora that have a creation date that falls within this date range. Dates should have format YYYY-MM-DD or YYYY-MM-DDThh-mm-ss.qqqZ and separated by ..',
      'cmodel' => 'Optionally, filters the objects found by collection/batchset/ids_file; only export objects with the given content model(s). If the content models start with - the objects with these content models will not be exported.',
      'skip_ids_file' => 'Optionally, the absolute filepath to a file containing a list of Islandora identifiers that will be skipped, so do not export these items. You can use an existing export file if the first column contains the item id.',
      'include_related' => 'Optionally, which related items will be exported also. By default, all related items will be exported when exporting a collection, via a ids_file or a solr_query. Allowed values are: all, no, collectionchild, bookpage, compoundchild.',
    ),
    'aliases' => array('export'),
    'examples' => array(
      'drush --user=admin islandora_export --collection=islandora:root --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --batchset=66 --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --ids_file=/path/to/idsfile.csv --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --ids_file=/path/to/idsfile.csv --ids_solr_key=mods_relatedItem_otherFormat_identifier_s --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --solr_query=catch_all_fields_mt:book --cmodel=islandora:bookCModel --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --solr_query=catch_all_fields_mt:book --cmodel=-islandora:pageCModel --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --solr_query=catch_all_fields_mt:book --cmodel=-islandora:pageCModel --skip_ids_file=/path/to/skipidsfile.csv --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
    ),
  );
  return $items;
}

/**
 * Implements drush_hook_COMMAND_validate().
 */
function drush_islandora_export_validate() {
  module_load_include('inc', 'islandora_export', 'includes/utilities');
  module_load_include('inc', 'islandora_export', 'includes/formatfile');

  $collections = drush_get_option('collection');
  $batchsets = drush_get_option('batchset');
  $idsfile = drush_get_option('ids_file');
  $idssolrkey = drush_get_option('ids_solr_key');
  $solrquery = drush_get_option('solr_query');
  $daterange = drush_get_option('date_range');
  $cmodels = drush_get_option('cmodel');
  $formatfile = drush_get_option('format_file');
  $directory = drush_get_option('directory');
  $skipidsfile = drush_get_option('skip_ids_file');
  $includerelated = drush_get_option('include_related');

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
  if (!islandora_export_validate_formatfile($formatfile)) {
    return FALSE;
  }

  // Check collection option.
  if (isset($collections)) {
    $colarray = explode(',', $collections);
    foreach ($colarray as $collection) {
      if (preg_match("/^([^>]+)>((?:page|item)number)=(\d+)(?:>pagesize=([1-9]\d*))?$/", $collection, $matches)) {
        $collection = $matches[1];
      }
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

  if (isset($idssolrkey)) {
    if (!isset($idsfile)) {
      return drush_set_error("ids_solr_key can only be used with the ids_file option");
    }
  }

  // Check solr_query option.
  if (isset($solrquery)) {
    if (isset($idsfile) || isset($batchsets) || isset($collections) || isset($daterange)) {
      return drush_set_error("solr_query cannot be combined with collection, batchset or ids_file.");
    }
  }

  // Check date_range option.
  if (isset($daterange)) {
    $dates = explode('..', $daterange, 2);
    $isvalidformat = TRUE; // be positive.
    if (count($dates) !== 2) {
      $isvalidformat = FALSE;
    }
    foreach ($dates as $date) {
      if ($date !== '' && !preg_match('/^[12][0-9]{3}-[01][0-9]-[0-3][0-9](?:T[0-2][0-9]:[0-5][0-9]:[0-5][0-9]\\.[0-9]{3}Z)?$/', $date)) {
        $isvalidformat = FALSE;
      }
    }
    if (!$isvalidformat) {
      return drush_set_error("date_range should contain 2 dates, separated by two points, e.g. 2017-01-01..2018-01-01.");
    }
  }

  // Check skip_ids_file option.
  if (isset($skipidsfile)) {
    if (!file_exists($skipidsfile)) {
      return drush_set_error("skip_ids_file '$skipidsfile' does not exist");
    }
  }

  // Check include_related option.
  if (isset($includerelated)) {
    $includerelatedarray = explode(',', $includerelated);
    $notallowed = array_diff($includerelatedarray, array('all', 'no', 'none', 'collectionchild', 'bookpage', 'compoundchild'));
    if (count($notallowed) > 0) {
      return drush_set_error("include_related contains '$notallowed[0]' which is not allowed, use all, no, bookpage, compoundchild.");
    }
  }

  return TRUE;
}

/**
 * Implements drush_hook_COMMAND().
 */
function drush_islandora_export() {
  module_load_include('inc', 'islandora_export', 'includes/utilities');

  $collections = drush_get_option('collection');
  $batchsets = drush_get_option('batchset');
  $idsfile = drush_get_option('ids_file');
  $idssolrkey = drush_get_option('ids_solr_key');
  $solrquery = drush_get_option('solr_query');
  $daterange = drush_get_option('date_range');
  $cmodels = drush_get_option('cmodel');
  $formatfile = drush_get_option('format_file');
  $directory = drush_get_option('directory');
  $skipidsfile = drush_get_option('skip_ids_file');
  $includerelated = drush_get_option('include_related');

  $filter = array();
  if (isset($cmodels)) {
    $cmodelarray = explode(',', $cmodels);
    foreach ($cmodelarray as $cmodel) {
      $prefix = substr($cmodel, 0, 1);
      if ($prefix === "-") {
        $filter['excluded_cmodels'][] = substr($cmodel, 1);
      }
      elseif ($prefix === "+") {
        $filter['included_cmodels'][] = substr($cmodel, 1);
      }
      else {
        $filter['included_cmodels'][] = $cmodel;
      }
    }
  }
  $filter['include_related'] = array('collectionchild' => FALSE, 'bookpage' => FALSE, 'compoundchild' => FALSE);
  if (isset($includerelated)) {
    $includerelatedarray = explode(',', $includerelated);
    if (in_array('all', $includerelatedarray)) {
      // this is the default
      foreach ($filter['include_related'] as $k => $v) {
        $filter['include_related'][$k] = TRUE;
      }
    }
    elseif (in_array('no', $includerelatedarray) || in_array('none', $includerelatedarray) ) {
      // nothing needs to change
    }
    else {
      foreach ($includerelatedarray as $k) {
        $filter['include_related'][$k] = TRUE;
      }
    }
  }
  else {
    foreach ($filter['include_related'] as $k => $v) {
      $filter['include_related'][$k] = TRUE;
    }
  }

  $doneids = array();
  if (isset($skipidsfile)) {
    $skipids = islandora_export_ids_from_file($skipidsfile);
    foreach ($skipids as $skipitemid) {
      $doneids[$skipitemid] = 1;
    }
  }

  $formatconfig = parse_ini_file($formatfile, TRUE);
  $settings = array('solronly' => TRUE);
  foreach ($formatconfig as $key => $config) {
    if ($key === 'exportformat') {
      continue;
    }
    if ($config['type'] === 'file') {
      $settings['solronly'] = FALSE;
      break;
    }
    if ($config['type'] === 'value' && $config['source']['type'] !== 'solr') {
      $settings['solronly'] = FALSE;
      break;
    }
  }

  if (isset($collections)) {
    $colarray = explode(',', $collections);
    foreach ($colarray as $collection) {
      if (preg_match("/^([^>]+)>((?:page|item)number)=(\d+)(?:>pagesize=(\d+))?$/", $collection, $matches)) {
        $collection = $matches[1];
        $settings[$matches[2]] = $matches[3];
        if (isset($matches[4])) {
          $settings['pagesize'] = $matches[4];
        }
      }
      $collectionobj = islandora_object_load($collection);
      if ($collectionobj && in_array('islandora:collectionCModel', $collectionobj->models)) {
        islandora_export_process_children($collectionobj, $filter, $formatconfig, $directory, $doneids, $settings);
      }
    }
  }
  if (isset($batchsets)) {
    $batchsetarray = explode(',', $batchsets);
    foreach ($batchsetarray as $batchset) {
      $itemids = islandora_batch_get_queued_items_for_set($batchset);
      foreach ($itemids as $itemid) {
        if (isset($doneids[$itemid])) {
          continue;
        }
        if ($settings['solronly']) {
          $obj = islandora_export_get_solr_object($itemid);
        }
        else {
          $obj = islandora_object_load($itemid);
        }
	if ($obj) {
          if (islandora_export_filter_should_export($obj, $filter)) {
	    islandora_export_object_export($obj, $formatconfig, $directory);
          }
          $doneids[$itemid] = 1;
        }
      }
    }
  }
  if (isset($idsfile)) {
    $ids = islandora_export_ids_from_file($idsfile);
    foreach ($ids as $anid) {
      if (isset($doneids[$anid])) {
        continue;
      }
      if (isset($idssolrkey)) {
        $customsolrquery = "$idssolrkey:($anid)";
        islandora_export_do_work_for_solr_query($customsolrquery, 0, FALSE, function($solrdoc, $current, $numfound) use (&$doneids, $solrquery, $settings, $filter, $formatconfig, $directory, $doneids, $settings) {
          if (islandora_export_filter_should_export($solrdoc, $filter)) {
            islandora_export_object_export($solrdoc, $formatconfig, $directory);
          }
          $itemid = $solrdoc['PID'];
          $doneids[$itemid] = 1;
          islandora_export_process_children($solrdoc, $filter, $formatconfig, $directory, $doneids, $settings);
        });
      }
      else {
        if ($settings['solronly']) {
          $obj = islandora_export_get_solr_object($anid);
        }
        else {
          $obj = islandora_object_load($anid);
        }
        if ($obj) {
          if (islandora_export_filter_should_export($obj, $filter)) {
            islandora_export_object_export($obj, $formatconfig, $directory);
          }
          $doneids[$anid] = 1;
          islandora_export_process_children($obj, $filter, $formatconfig, $directory, $doneids, $settings);
        }
      }
    }
  }
  if (isset($solrquery)) {
    islandora_export_do_work_for_solr_query($solrquery, 0, FALSE, function($solrdoc, $current, $numfound) use (&$doneids, $solrquery, $settings, $filter, $formatconfig, $directory, $doneids, $settings) {
      if ($current === 0) {
        drush_log(t("Found @numfound objects for query @query", array("@numfound" => $numfound, "@query" => $solrquery)), 'ok');
      }
      $objid = $solrdoc['PID'];
      if (isset($doneids[$objid])) {
        return;
      }
      if ($settings['solronly']) {
        if (islandora_export_filter_should_export($solrdoc, $filter)) {
          islandora_export_object_export($solrdoc, $formatconfig, $directory);
        }
        $doneids[$objid] = 1;
        islandora_export_process_children($solrdoc, $filter, $formatconfig, $directory, $doneids, $settings);
      }
      else {
        $obj = islandora_object_load($objid);
        if ($obj) {
          if (islandora_export_filter_should_export($obj, $filter)) {
            islandora_export_object_export($obj, $formatconfig, $directory);
          }
          $doneids[$objid] = 1;
          islandora_export_process_children($obj, $filter, $formatconfig, $directory, $doneids, $settings);
        }
      }
    });
  }

  if (isset($daterange)) {
    $query = <<<EOQ
SELECT ?object ?cd ?title
FROM <#ri>
WHERE {
  ?object <fedora-model:label> ?title ;
  <info:fedora/fedora-system:def/model#createdDate> ?cd ;
  <fedora-model:hasModel> ?model ;
  <fedora-model:state> <fedora-model:Active> .
  !filters
}
ORDER BY ?cd
!limit
EOQ;
    $tuque = islandora_get_tuque_connection();
    if ($tuque) {
      $dates = explode('..', $daterange, 2);
      $startcreationdate = $dates[0];
      $endcreationdate = $dates[1];
      $limit = 2500;
      while (TRUE) {
        if (strlen($startcreationdate) > 0 && strlen($endcreationdate) > 0) {
          $filters['cd'] = "FILTER(?cd > '{$startcreationdate}'^^xsd:dateTime && ?cd < '{$endcreationdate}'^^xsd:dateTime)";
        }
        elseif (strlen($startcreationdate) > 0) {
          $filters['cd'] = "FILTER(?cd > '{$startcreationdate}'^^xsd:dateTime)";
        }
        elseif (strlen($endcreationdate) > 0) {
          $filters['cd'] = "FILTER(?cd < '{$endcreationdate}'^^xsd:dateTime)";
        }
        $query_string = format_string($query, array(
          '!filters' => implode(' ', $filters),
          '!limit' => "LIMIT $limit"));
        $results = $tuque->repository->ri->sparqlQuery($query_string);
        if (empty($results)) {
          break;
        }
        else {
          foreach ($results as $result) {
            $pid = $result['object']['value'];
            $startcreationdate = $result['cd']['value'];
            if (isset($doneids[$pid])) {
              continue;
            }
            if ($settings['solronly']) {
              $obj = islandora_export_get_solr_object($pid);
            }
            else {
              $obj = islandora_object_load($pid);
            }
            if ($obj) {
              if (islandora_export_filter_should_export($obj, $filter)) {
                islandora_export_object_export($obj, $formatconfig, $directory);
              }
              $doneids[$pid] = 1;
            }
          }
          drush_log(t("Exported uptil date @date.", array('@date' => $startcreationdate)), 'info');
        }
      }
    }
  }
}

