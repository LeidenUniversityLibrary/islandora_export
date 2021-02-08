# Islandora Export

## Introduction

Islandora Export is a module to export data from Islandora via a drush `islandora_export` command. The drush command needs the following arguments:
 - what: the items that need to be exported. You can identify the items with the following arguments:
   - `--collection=`*collection_id* : Optionally, one or more collection IDs, separated by comma's
   - `--batchset=`*batchsetid* : Optionally, one or more batchset IDs, separated by comma's.
   - `--ids_file=`*path/to/idsfile.csv* : Optionally, the absolute filepath to a file containing a list of Islandora identifiers.
   - `--solr_query=`*solr_query* : Optionally, a Solr query to find the items to export. Cannot be combined with collection, batchset or `ids_file`.
   - `--date_range=`*YYYY-MM-DD..YYYY-MM_DD* : Optionally, exports the items from Islandora that have a creation date that falls within this date range. Dates should have format YYYY-MM-DD or YYYY-MM-DDThh-mm-ss.qqqZ and separated by ..
 - further refine/expand which items are exported:
   - `--include_related=`*all,no,collectionchild,bookpage,compoundchild* : Optionally, which related items will be exported also. By default, all directly related items will be exported when exporting a collection, but no related items will be exported otherwise. Allowed values are: all, no, collectionchild, bookpage, compoundchild.
   - `--cmodel=`*cmodel* : Optionally, filters the objects found by collection/batchset/`ids_file`/`solr_query`/`date_range`; only export objects with the given content model(s). If the content models start with - the objects with these content models will not be exported
 - solr options:
   - `--ids_solr_key=`*solr_key* : Optionally, the Solr key or keys to use for searching the IDs of the ids_file. Can only be used with ids_file option.
   - `--solr_sort=`*solr sort* : Optionally, a Solr sort option. Only applicable when using solr_query.
   - `--solr_start=`*number* : Optionally, at which item to start with a Solr export. Best used together with solr_sort. Defaults to 0.
   - `--solr_limit=`*number* : Optionally, the size of the batches used for Solr queries. Defaults to 50.
 - how the export should format the output is defined by an ini file. The format of this ini file is explained below.
   - `--format_file=`*/absolute/path/to/format/file*
 - where the output should be placed:
   - `--directory=`*/absolute/path/to/empty/output/directory/* : this should be an absolute path the an existing, empty output directory. If is is not empty, files can be overwritten or appended to.
 - which ids not to include:
   - `--skip_ids_file=`*path/to/idsfile.csv* : Optionally, the absolute filepath to a file containing a list of Islandora identifiers that will be skipped, so do not export these items. You can use an existing export file if the first column contains the item id.

For now, this module can only output data in CSV format. This is done to a file named `export.csv` in the given output directory. The columns and the content of the columns can be defined in the ini file, as explained below.

## drush examples

The following are examples of how to use this drush command. Please note that you should run this command as a user with sufficient rights.

```
drush --user=admin islandora_export --collection=islandora:root --format_file=/path/to/format.ini --directory=/path/to/empty/directory
drush --user=admin export --batchset=66 --format_file=/path/to/format.ini --directory=/path/to/empty/directory
drush --user=admin export --ids_file=/path/to/idsfile.csv --format_file=/path/to/format.ini --directory=/path/to/empty/directory
drush --user=admin export --ids_file=/path/to/idsfile.txt --cmodel=islandora:bookCModel --format_file=/path/to/format.ini --directory=/path/to/empty/directory
drush --user=admin export --solr_query=catch_all_fields_mt:book --cmodel=islandora:bookCModel --format_file=/path/to/format.ini --directory=/path/to/empty/directory
drush --user=admin export --solr_query=catch_all_fields_mt:book --cmodel=-islandora:pageCModel --format_file=/path/to/format.ini --directory=/path/to/empty/directory
drush --user=admin export --solr_query=catch_all_fields_mt:book --cmodel=-islandora:pageCModel --skip_ids_file=/path/to/skipidsfile.csv --format_file=/path/to/format.ini --directory=/path/to/empty/directory
drush --user=admin export --solr_query=catch_all_fields_mt:book --cmodel=-islandora:pageCModel --solr_start=1000 --solr_limit=100 --solr_sort="timestamp desc" --format_file=/path/to/format.ini --directory=/path/to/empty/directory
```

## format ini file

The format ini file is a file in the PHP INI format, see https://en.wikipedia.org/wiki/INI_file for more information about this format. PHP has some special features, such as support for multiple values for one key by using key[] or key[attr].
See the `example_ini_files` directory for examples.

The format ini file starts with a section named `exportformat` that defines the export format. The keys available in this section are:
 - `type` : mandatory key that defines the general format of the output file. Currently, only "CSV" is a valid value.
 - `separator` : mandatory key that defines the separator for the columns of the CSV file. Should be one character.
 - `columns[]` : mandatory multi-valued key. Repeat this key for each column. The value is the name of the column in the exported CSV file. There should be a section in the ini file with the same name that defines the values for this column.
 - `columntypes[type]` : optional, key that binds a type to a content model identifier. Sections with name `type:columnname` are only used for objects with the defined content model.

The other sections defines the values of the columns. The section name is the same as defined in the `columns[]`, possibly prepended by the type as defined in `columntypes[]`. Type and columnname are separated by a colon.
These sections can have the following keys:
 - `type` : mandatory, can have the following values: `string`, `value` or `file`. The value of this key influences which other keys are possible.
 - `string` : mandatory for type=string, do not use for other types. The value of this key is used as-is.
 - `source[type]` : mandatory for type=value or type=file, do not use for type=string. This key can have the following values: `datastream` or `property` or `solr`. The value of this key influences which other keys are possible.
 - `source[property]` : mandatory for source[type]=property, do not use otherwise. This defines the property to use for the current object. Valid values are: `creationdate`, `creationdatetime`, `id`, `label`, `modifydate`, `modifydatetime`, `owner`, `state` (A/I/D), `cmodels` (comma separated list of cmodels), `parents` (comma separated list of pids of parents).
 - `source[dsid]` : mandatory for source[type]=datastream, do not use otherwise. This defines the datastream to use for the current object.
 - `solr[key]` : mandatory for source[type]=solr, do not use otherwise. This defines the Solr key to use as the value.
 - `extract[type]` : optional, only use for type=value and source[type]=datastream. This defines what type of extraction of data should be used for the datastream. This key can have the following values: `property` or `xpath`. The value of this key influences which other keys are possible.
 - `extract[property]` : mandatory for extract[type]=property, do not use otherwise. This defines which property should be extracted from the datastream. Valid values are: `checksum`, `checksumtype`, `controlgroup` (Inline (X)ML, (M)anaged Content, (R)edirect, or (E)xternal Referenced), `creationdate`, `creationdatetime`, `id`, `label`, `mimetype`, `size`, `state` (A/I/D), `url` (only for controlgroup R or E), `extension`.
 - `extract[xpath]` : mandatory for extract[type]=xpath, do not use otherwise. Only use with datastreams in XML format, like 'MODS' or 'DC'. The value should be a valid XPath to a single value inside the datastream. Use `extract[namespaces]` to supply the namespaces for the xpath.
 - `extract[namespaces]` : optional for extract[type]=xpath, do not use otherwise. The value should be one or more prefix URI pairs separated by semicolons, for example: "mods http://www.loc.gov/mods/v3".
 - `outputdirectory` : optional for type=file. This is the output directory to which the file is saved. This is a directory relative to the directory given as argument for the drush command. Use one of these versions:
   - `outputdirectory[string]` : The value is used as-is.
   - `outputdirectory[like]` : The value is the name of another section of type=value. The value from that section is used as the outputdirectory. Characters in this value other than a-z, A-Z, 0-9, . (point), _ (underscore) or - (hyphen) are replaced by an underscore. If the directrory does not exist already, it is made.
 - `outputfilename` : mandatory for type=file. This is the output filename under which the file is saved. Use one of these versions:
   - `outputfilename[string]` : The value is used as-is.
   - `outputfilename[like]` : The value is the name of another section of type=value. The value from that section is used as the outputfilename. Characters in this value other than a-z, A-Z, 0-9, . (point), _ (underscore) or - (hyphen) are replaced by an underscore.
 - `outputextension` : optional for type=file. This is the output extension of the filename under which the file is saved. Use one of these versions:
   - `outputextension[string]` : The value is used as-is.
   - `outputextension[like]` : The value is the name of another section of type=value. The value from that section is used as the outputextension. Characters in this value other than a-z, A-Z, 0-9, . (point), _ (underscore) or - (hyphen) are replaced by an underscore.

Note: when using only Solr source types, the export will be significantly faster. This is because all the exported data can be requested from Solr. So if you don't want to export files but only metadata that is available in Solr, it is recommended to only use Solr source types. See `example_ini_files/csv_solr_id_title_cmodel.ini`.

## Requirements

This module requires the following modules/libraries:

* [Islandora](https://github.com/islandora/islandora)

## Installation

 Install as usual, see [this](https://drupal.org/documentation/install/modules-themes/modules-7) for further information.

## Configuration

No further configuration is needed to use this module.

## Maintainers/Sponsors

Current maintainers:

* [Lucas van Schaik](https://github.com/lucasvanschaik)

## Development

If you would like to contribute to this module, please contact the current maintainer.

## License

[GPLv3](LICENSE.txt)
Copyright 2017-2018 Leiden University Library

