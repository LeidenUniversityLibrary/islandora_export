<?php
/**
 * @file
 * Functions for exporting Islandora content via drush.
 *
 *
 *  Copyright 2017-2018 Leiden University Library
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
      'format_file' => 'This option or the next is mandatory. The absolute filepath to an ini file containing the format of the export. See documentation for specifics.',
      'format_string' => 'This option or the previous is mandatory. One or more solr field names, separated by comma\'s.',
      'directory' => 'Mandatory, the absolute filepath to an empty directory to export to.',
      'collection' => 'Optionally, one or more collection IDs, separated by comma\'s.',
      'batchset' => 'Optionally, one or more batchset IDs, separated by comma\'s.',
      'ids_file' => 'Optionally, the absolute filepath to a file containing a list of Islandora identifiers that will be exported.',
      'ids_solr_key' => 'Optionally, the Solr key or keys (seperated by comma\'s) to use for searching the IDs of the ids_file. Can only be used with ids_file option.',
      'solr_query' => 'Optionally, a Solr query to find the items to export. Cannot be combined with collection, batchset or ids_file.',
      'solr_sort' => 'Optionally, a Solr sort option. Only applicable when using solr_query.',
      'solr_start' => 'Optionally, at which item to start with a Solr export. Best used together with solr_sort. Defaults to 0.',
      'solr_limit' => 'Optionally, the size of the batches used for Solr queries. Defaults to 50.',
      'creation_date_range' => 'Optionally, exports the items from Islandora that have a creation date that falls within this date range. Dates should have format YYYY-MM-DD or YYYY-MM-DDThh-mm-ss.qqqZ and separated by ..',
      'modification_date_range' => 'Optionally, exports the items from Islandora that have a creation date that falls within this date range. Dates should have format YYYY-MM-DD or YYYY-MM-DDThh-mm-ss.qqqZ and separated by ..',
      'cmodel' => 'Optionally, filters the objects found by collection/batchset/ids_file/solr_query/date_ranges; only export objects with the given content model(s). If the content models start with - the objects with these content models will not be exported.',
      'skip_ids_file' => 'Optionally, the absolute filepath to a file containing a list of Islandora identifiers that will be skipped, so do not export these items. You can use an existing export file if the first column contains the item id.',
      'include_related' => 'Optionally, which related items will be exported also. By default, all directly related items will be exported when exporting a collection, but no related items will be exported otherwise. Allowed values are: all, no, collectionchild, bookpage, compoundchild.',
    ),
    'aliases' => array('export'),
    'examples' => array(
      'drush --user=admin islandora_export --collection=islandora:root --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --batchset=66 --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --ids_file=/path/to/idsfile.csv --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --ids_file=/path/to/idsfile.csv --ids_solr_key=mods_identifier_s,mods_relatedItem_otherFormat_identifier_s --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --solr_query=catch_all_fields_mt:book --cmodel=islandora:bookCModel --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --solr_query=catch_all_fields_mt:book --cmodel=-islandora:pageCModel --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --solr_query=catch_all_fields_mt:book --cmodel=-islandora:pageCModel --skip_ids_file=/path/to/skipidsfile.csv --format_file=/path/to/format.ini --directory=/path/to/empty/directory',
      'drush --user=admin export --solr_query=catch_all_fields_mt:book --format_string=PID,fgs_label_s,mods_identifier_s --directory=/path/to/empty/directory',
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
  $solrsort = drush_get_option('solr_sort');
  $solrlimit = drush_get_option('solr_limit');
  $solrstart = drush_get_option('solr_start');
  $creationdaterange = drush_get_option('creation_date_range');
  $modificationdaterange = drush_get_option('modification_date_range');
  $cmodels = drush_get_option('cmodel');
  $formatfile = drush_get_option('format_file');
  $formatstring = drush_get_option('format_string');
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
  // Check format_file and format_string options.
  if (!isset($formatfile) && !isset($formatstring)) {
    return drush_set_error("Argument format_file or format_string is mandatory!");
  }
  if (isset($formatfile) && !islandora_export_validate_formatfile($formatfile)) {
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
    if (isset($idsfile) || isset($batchsets) || isset($collections) || isset($creationdaterange) || isset($modificationdaterange)) {
      return drush_set_error("solr_query cannot be combined with collection, batchset, ids_file or dateranges.");
    }
  }

  // Check other solr options.
  if (isset($solrstart)) {
    if (!isset($solrquery)) {
      return drush_set_error("solr_start can only be used together with solr_query.");
    }
    if (filter_var($solrstart, FILTER_VALIDATE_INT, array('options' => array('min_range' => 0))) === FALSE) {
      return drush_set_error("solr_start should be a whole positive number or zero.");
    }
  }
  if (isset($solrlimit)) {
    if (filter_var($solrlimit, FILTER_VALIDATE_INT, array('options' => array('min_range' => 1))) === FALSE) {
      return drush_set_error("solr_limit should be a whole positive number greater than zero.");
    }
  }
  if (isset($solrsort)) {
    if (!isset($solrquery)) {
      return drush_set_error("solr_sort can only be used together with solr_query.");
    }
  }

  // Check date_range option.
  if (isset($creationdaterange) || isset($modificationdaterange)) {
    if (isset($creationdaterange) && isset($modificationdaterange)) {
      return drush_set_error("creation_date_range and modification_date_range cannot be combined.");
    }
    $daterange = isset($creationdaterange) ? $creationdaterange : $modificationdaterange;
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
      $type = isset($creationdaterange) ? 'creation' : 'modification';
      return drush_set_error("{$type}_date_range should contain 2 dates, separated by two points, e.g. 2017-01-01..2018-01-01.");
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
      return drush_set_error("include_related contains '$notallowed[0]' which is not allowed, use all, no/none, collectionchild, bookpage, compoundchild.");
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
  $creationdaterange = drush_get_option('creation_date_range');
  $modificationdaterange = drush_get_option('modification_date_range');
  $cmodels = drush_get_option('cmodel');
  $formatfile = drush_get_option('format_file');
  $formatstring = drush_get_option('format_string');
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
  $filter['include_related'] = array('collectionchild' => isset($collections), 'bookpage' => FALSE, 'compoundchild' => FALSE);
  if (isset($includerelated)) {
    $includerelatedarray = explode(',', $includerelated);
    if (in_array('all', $includerelatedarray)) {
      foreach ($filter['include_related'] as $k => $v) {
        $filter['include_related'][$k] = TRUE;
      }
    }
    elseif (in_array('no', $includerelatedarray) || in_array('none', $includerelatedarray) ) {
      // this is the default
      // nothing needs to change
    }
    else {
      foreach ($includerelatedarray as $k) {
        $filter['include_related'][$k] = TRUE;
      }
    }
  }

  $doneids = array();
  if (isset($skipidsfile)) {
    $skipids = islandora_export_ids_from_file($skipidsfile);
    foreach ($skipids as $skipitemid) {
      $doneids[$skipitemid] = 1;
    }
  }

  if (isset($formatfile)) {
    $formatconfig = parse_ini_file($formatfile, TRUE);
  }
  else {
    $fields = preg_split("/ *, */", $formatstring, -1, PREG_SPLIT_NO_EMPTY);
    $formatconfig = array(
      'exportformat' => array(
        'type' => 'CSV',
        'separator' => ';',
        'columns => $fields,
      ),
    );
    foreach ($fields as $field) {
      $formatconfig[$field] = array(
        'type' => 'value',
        'source' => array('type' => 'solr'),
        'solr' => array('key' => $field),
      );
    }
  }
  $settings = array('solronly' => TRUE);
  $solrfields = array();
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
    if (isset($config['solr']['key'])) {
      $solrfield = $config['solr']['key'];
      $solrfields[$solrfield] = $solrfield;
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
        $customsolrquery = '';
        foreach (explode(',', $idssolrkey) as $solrkey) {
          if (strlen($customsolrquery) > 0) {
            $customsolrquery .= ' OR ';
          }
          $customsolrquery .= "$solrkey:\"$anid\"";
        }
        islandora_export_do_work_for_solr_query($customsolrquery, array(), FALSE, function($solrdoc, $current, $numfound) use (&$doneids, $solrquery, $settings, $filter, $formatconfig, $directory) {
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
    $solroptions = array();
    if (drush_get_option("solr_start") !== NULL) {
      $solroptions["start"] = drush_get_option("solr_start");
    }
    if (drush_get_option("solr_limit") !== NULL) {
      $solroptions["limit"] = drush_get_option("solr_limit");
    }
    if (drush_get_option("solr_sort") !== NULL) {
      $solroptions["sort"] = drush_get_option("solr_sort");
    }
    if (count($solrfields) > 0) {
      $solroptions["fields"] = $solrfields;
    }
    islandora_export_do_work_for_solr_query($solrquery, $solroptions, FALSE, function($solrdoc, $current, $numfound) use (&$doneids, $solrquery, $settings, $filter, $formatconfig, $directory) {
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

  if (isset($creationdaterange) || isset($modificationdaterange)) {
    $datetype = isset($creationdaterange) ? '<info:fedora/fedora-system:def/model#createdDate>' : '<info:fedora/fedora-system:def/view#lastModifiedDate>';
    $query = <<<EOQ
SELECT ?object ?cd ?title
FROM <#ri>
WHERE {
  ?object <fedora-model:label> ?title ;
  $datetype ?cd ;
  <fedora-model:hasModel> ?model ;
  <fedora-model:state> <fedora-model:Active> .
  !filters
}
ORDER BY ?cd
!limit
EOQ;
    $tuque = islandora_get_tuque_connection();
    if ($tuque) {
      $daterange = isset($creationdaterange) ? $creationdaterange : $modificationdaterange;
      $dates = explode('..', $daterange, 2);
      $startdate = $dates[0];
      $enddate = $dates[1];
      $limit = 2500;
      while (TRUE) {
        if (strlen($startdate) > 0 && strlen($enddate) > 0) {
          $filters['cd'] = "FILTER(?cd > '{$startdate}'^^xsd:dateTime && ?cd < '{$enddate}'^^xsd:dateTime)";
        }
        elseif (strlen($startdate) > 0) {
          $filters['cd'] = "FILTER(?cd > '{$startdate}'^^xsd:dateTime)";
        }
        elseif (strlen($enddate) > 0) {
          $filters['cd'] = "FILTER(?cd < '{$enddate}'^^xsd:dateTime)";
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
            $startdate = $result['cd']['value'];
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
          drush_log(t("Exported uptil date @date.", array('@date' => $startdate)), 'info');
        }
      }
    }
  }
}

