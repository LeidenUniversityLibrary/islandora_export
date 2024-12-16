<?php
/**
 * @file
 * utilities.inc.
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

function islandora_export_process_children($obj, $filter, $formatconfig, $directory, &$doneids, $settings = array()) {
  if (!$obj) {
    return;
  }
  $objid = ($obj instanceof FedoraObject)?$obj->id:$obj['PID'];
  $cmodels = ($obj instanceof FedoraObject)?$obj->models:(isset($obj['content_models'])?$obj['content_models']:array());
  if (in_array('islandora:collectionCModel', $cmodels)) {
    if ($filter['include_related']['collectionchild'] === FALSE) {
      return;
    }
    module_load_include('inc', 'islandora', 'includes/utilities');
    $pagenumber = isset($settings['pagenumber'])?$settings['pagenumber']:0;
    $pagesize = isset($settings['pagesize'])?$settings['pagesize']:100;
    if (isset($settings['itemnumber'])) {
      $pagenumber = (int)($settings['itemnumber']/$pagesize);
    }
    $newsettings = array_diff_key($settings, array_flip(array('pagenumber','itemnumber')));
    if ($settings['solronly']) {
      $solrquery = "RELS_EXT_isMemberOfCollection_uri_ms:\"info:fedora/$objid\"";
      $solroptions = array("start" => ($pagenumber * $pagesize));
      islandora_export_do_work_for_solr_query($solrquery, $solroptions, FALSE, function($solrobj, $current, $numfound) use (&$doneids, $filter, $formatconfig, $directory, $newsettings) {
        if (isset($doneids[$solrobj['PID']])) {
          return;
        }
        if (islandora_export_filter_should_export($solrobj, $filter)) {
          islandora_export_object_export($solrobj, $formatconfig, $directory);
        }
        $doneids[$solrobj['PID']] = 1;
        islandora_export_process_children($solrobj, $filter, $formatconfig, $directory, $doneids, $newsettings);
      });
    }
    else {
      if (!($obj instanceof FedoraObject)) {
        $obj = islandora_object_load($objid);
      }
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
            islandora_export_process_children($cobj, $filter, $formatconfig, $directory, $doneids, $newsettings);
          }
        }
      }
    }
  }
  if (in_array('islandora:bookCModel', $cmodels)) {
    if ($filter['include_related']['bookpage'] === FALSE) {
      return;
    }
    if ($settings['solronly']) {
      $solrquery = format_string('@field:"@pid"', array(
         '@field' => variable_get('islandora_solr_member_of_field', 'RELS_EXT_isMemberOf_uri_ms'),
         '@pid' => "info:fedora/$objid"));
      islandora_export_do_work_for_solr_query($solrquery, array(), FALSE, function($solrobj, $current, $numfound) use (&$doneids, $filter, $formatconfig, $directory, $settings) {
        if (isset($doneids[$solrobj['PID']])) {
          return;
        }
        if (islandora_export_filter_should_export($solrobj, $filter)) {
          islandora_export_object_export($solrobj, $formatconfig, $directory);
        }
        $doneids[$solrobj['PID']] = 1;
      });
    }
    else {
      module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
      if (!($obj instanceof FedoraObject)) {
        $obj = islandora_object_load($objid);
      }
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
  }
  if (in_array('islandora:compoundCModel', $cmodels)) {
    if ($filter['include_related']['compoundchild'] === FALSE) {
      return;
    }
    if ($settings['solronly']) {
      $solrquery = "RELS_EXT_isConstituentOf_uri_ms:\"info:fedora/$objid\"";
      islandora_export_do_work_for_solr_query($solrquery, array(), FALSE, function($solrobj, $current, $numfound) use (&$doneids, $filter, $formatconfig, $directory, $settings) {
        if (isset($doneids[$solrobj['PID']])) {
          return;
        }
        if (islandora_export_filter_should_export($solrobj, $filter)) {
          islandora_export_object_export($solrobj, $formatconfig, $directory);
        }
        $doneids[$solrobj['PID']] = 1;
        islandora_export_process_children($solrobj, $filter, $formatconfig, $directory, $doneids, $settings);
      });
    }
    else {
      $compoundparts = islandora_compound_object_get_parts($objid);
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
}

function islandora_export_filter_should_export($obj, $filter) {
  $cmodels = ($obj instanceof FedoraObject)?$obj->models:(isset($obj['content_models'])?$obj['content_models']:array());
  if (isset($filter['excluded_cmodels'])) {
    if (array_intersect($filter['excluded_cmodels'], $cmodels)) {
      return FALSE;
    }
  }
  if (isset($filter['included_cmodels'])) {
    if (!array_intersect($filter['included_cmodels'], $cmodels)) {
      return FALSE;
    }
  }
  return TRUE;
}

function islandora_export_object_export($obj, array $formatconfig, $outputdirectory) {
  if ($formatconfig['exportformat']['type'] === 'CSV') {
    if (isset($formatconfig['exportformat']['file'])) {
      $csvfile = $formatconfig['exportformat']['file'];
    }
    else {
      $csvfile = rtrim($outputdirectory, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . 'export.csv';
    }
    if ($csvfile === '-') {
      $csvfilehandle = fopen("php://stdout", "w");
      static $header_printed = FALSE;
      if (!$header_printed) {
        if (!islandora_export_print_header($csvfilehandle, $csvfile, $formatconfig)) {
          return FALSE;
        }
        $header_printed = TRUE;
      }
    }
    else {
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
        if (!islandora_export_print_header($csvfilehandle, $csvfile, $formatconfig)) {
          return FALSE;
        }
      }
    }
    $ok = islandora_export_object_export_csv($obj, $formatconfig, $outputdirectory, $csvfilehandle);
    fclose($csvfilehandle);
    if ($ok === FALSE) {
      return FALSE;
    }
  }
  drush_log(t("Exported object @id succesfully", array('@id' => (isset($obj['PID'])?$obj['PID']:$obj->id))), 'ok');
}

function islandora_export_print_header($csvfilehandle, $csvfile, $formatconfig) {
  $separator = $formatconfig['exportformat']['separator'];
  $columns = $formatconfig['exportformat']['columns'];
  if (fputcsv($csvfilehandle, $columns, $separator) === FALSE) {
    return drush_set_error("Failed to write to file $csvfile");
  }
  return TRUE;
}

function islandora_export_object_export_csv($obj, array $formatconfig, $outputdirectory, $csvfilehandle) {
  $separator = $formatconfig['exportformat']['separator'];
  $columnnames = $formatconfig['exportformat']['columns'];

  $columnvalues = array();
  foreach ($columnnames as $columnname) {
    unset($columnconfig);
    if (isset($formatconfig['exportformat']['columntypes'])) {
      $columntypes = $formatconfig['exportformat']['columntypes'];
      $cmodels = ($obj instanceof FedoraObject)?$obj->models:(isset($obj['content_models'])?$obj['content_models']:array());
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
    if (!($obj instanceof FedoraObject)) {
      $obj = islandora_object_load($obj['PID']);
    }
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
  elseif ($source['type'] === 'solr') {
    $key = $columnconfig['solr']['key'];
    if ($obj instanceof FedoraObject) {
      $solrobj = islandora_export_get_solr_object($obj->id);
      if (isset($solrobj[$key])) {
        $value = $solrobj[$key];
      }
    }
    else {
      if (isset($obj[$key])) {
        $value = $obj[$key];
      }
    }
    if (isset($value) && is_array($value)) {
      $value = implode(",", $value);
    }
  }
  elseif ($source['type'] === 'handle') {
    module_load_include('inc', 'islandora_handle', 'includes/handle');
    static $handler;
    if (!isset($handler)) {
      $handler_class = islandora_handle_retrieve_selected_handler();
      $handler = new $handler_class();
    }
    if (!($obj instanceof FedoraObject)) {
      $obj = islandora_object_load($obj['PID']);
    }
    if ($columnconfig['handle']['type'] === 'handle') {
      $value = $handler->getFullHandle($obj);
    }
    elseif ($columnconfig['handle']['type'] === 'target') {
      $value = $handler->readHandle($obj);
    }
  }
  elseif ($source['type'] === 'drupal') {
    $type = $columnconfig['drupal']['type'];
    $pid = ($obj instanceof FedoraObject) ? $obj->id : $obj['PID'];
    if ($type === 'fedora_path') {
      $arg = isset($columnconfig['drupal']['argument']) ? $columnconfig['drupal']['argument'] : '';
      list($dsid, $version) = explode('.', $arg);
      $dsf = variable_get('islandora_solr_datastream_id_field', 'fedora_datastreams_ms');
      if (empty($dsid) || (($obj instanceof FedoraObject) && isset($obj[$dsid])) || (!($obj instanceof FedoraObject) && isset($obj[$dsf]) && array_search($dsid, $obj[$dsf]) !== FALSE)) {
        $value = islandora_export_get_fedora_path($pid, $dsid, $version);
      }
    }
    else {
      $value = url("islandora/object/$pid", array('absolute' => (substr($type, 0, 5) === 'full_'), 'alias' => (substr($type, -4) === 'path')));
    }
  }
  return $value;
}

function islandora_export_object_property($obj, $property) {
  if (!($obj instanceof FedoraObject)) {
    $obj = islandora_object_load($obj['PID']);
  }
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
    if (!($obj instanceof FedoraObject)) {
      $obj = islandora_object_load($obj['PID']);
    }
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

function islandora_export_do_work_for_solr_query($solrquery, $solroptions, $alter_results, $workfunction) {
  $start = isset($solroptions['start'])?$solroptions['start']:0;
  $current = $start;
  $numfound = $start + 1;
  $cmf = variable_get('islandora_solr_content_model_field', 'RELS_EXT_hasModel_uri_ms');
  if (isset($solroptions['fields'])) {
    $fl = $solroptions['fields'];
    $dsf = variable_get('islandora_solr_datastream_id_field', 'fedora_datastreams_ms');
    foreach (array('PID', $cmf, $dsf) as $mandatory) {
      if (!isset($fl[$mandatory]) || !in_array($mandatory, $fl)) {
        $fl[$mandatory] = $mandatory;
      }
    }
  }
  while ($current < $numfound) {
    $qp = new IslandoraSolrQueryProcessor();
    $qp->buildQuery($solrquery);
    $qp->solrLimit = isset($solroptions['limit'])?$solroptions['limit']:50;
    $qp->solrStart = $current;
    $qp->solrParams['fq'] = array();
    if (isset($solroptions['sort'])) {
      $qp->solrParams['sort'] = $solroptions['sort'];
    }
    if (isset($fl)) {
      $qp->solrParams['fl'] = implode(',', $fl);
    }
    $qp->executeQuery($alter_results);
    $r = $qp->islandoraSolrResult;
    $numfound = $r['response']['numFound'];
    $len = count($r['response']['objects']);
    for ($i = 0; $i < $len; $i++) {
      $solrobj = $r['response']['objects'][$i];
      $solrdoc = $solrobj['solr_doc'];
      $cmodels = isset($solrobj[$cmf])?$solrobj[$cmf]:array();
      foreach ($cmodels as $cmodel) {
        $solrdoc['content_models'][] = str_replace('info:fedora/', '', $cmodel);
      }
      $solrdoc['PID'] = $solrobj['PID'];
      $workfunction($solrdoc, $current, $numfound);
      $current++;
    }
  }
}

function islandora_export_get_solr_object($pid) {
  $pid2solrdoc = &drupal_static(__FUNCTION__, array());

  if (!array_key_exists($pid, $pid2solrdoc)) {
    $solrquery = "PID:(" . islandora_solr_lesser_escape($pid) . ")";
    $solrdoc = FALSE;
    islandora_export_do_work_for_solr_query($solrquery, array('limit' => 1), FALSE, function($solrobj, $current, $numfound) use (&$solrdoc) {
      $solrdoc = $solrobj;
    });
    // only cache the last value. This is the only thing
    // needed since we are exporting 1 object at the time.
    array_pop($pid2solrdoc);
    $pid2solrdoc[$pid] = $solrdoc;
  }

  return $pid2solrdoc[$pid];
}

function islandora_export_get_fedora_path($pid, $dsid = NULL, $version = NULL) {
  if ($dsid) {
    if (!$version) {
      $version = 0;
    }
    $file = "info:fedora/$pid/$dsid/$dsid.$version";
    $dir = 'datastreamStore';
  }
  else {
    $file = "info:fedora/$pid";
    $dir = 'objectStore';
  }
  $md5 = md5($file);
  $subdir = substr($md5, 0, 2);
  return $dir . '/' . $subdir . '/' . urlencode($file);
}
