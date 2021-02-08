<?php
/**
 * @file
 * formatfile.inc.
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

function islandora_export_validate_formatfile($formatfile) {
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
  return TRUE;
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
    if ($sourcetype !== 'property' && $sourcetype !== 'datastream' && $sourcetype !== 'solr' && $sourcetype !== 'handle') {
      return drush_set_error("format_file '$formatfile' section [$column]: source[type] should be either 'property' or 'datastream' or 'solr' or 'handle'");
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
      elseif (isset($formatconfig[$column]['solr'])) {
        if (!isset($formatconfig[$column]['solr']['key'])) {
          return drush_set_error("format_file '$formatfile' section [$column]: solr[key] should exist");
        }
      }
      elseif (isset($formatconfig[$column]['handle'])) {
        if (!isset($formatconfig[$column]['handle']['type'])) {
          return drush_set_error("format_file '$formatfile' section [$column]: handle[type] should exist");
        }
        if (!in_array($formatconfig[$column]['handle']['type'], array('handle', 'target'))) {
          return drush_set_error("format_file '$formatfile' section [$column]: handle[type] should be either 'handle' or 'target'");
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


