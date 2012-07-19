<?php

/**
 * Function: get_collections_as_array
 *
 * Description: Returns an associative array of all collection objects in Fedora instance
 *
 * @return array
 */
function get_collections_as_array() {
  module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
  $restricted = variable_get('fedora_namespace_restriction_enforced', TRUE);
  $allowed_string = variable_get('fedora_pids_allowed', 'default: demo: changeme: islandora:');
  $namespaces = explode(':', $allowed_string);
  $query = 'select $object $title from <#ri>
                            where ($object <dc:title> $title
                              and $object <info:fedora/fedora-system:def/model#hasModel> <info:fedora/islandora:collectionCModel>
                              and $object <fedora-model:state> <info:fedora/fedora-system:def/model#Active>)
                            order by $title';
  $url = variable_get('fedora_repository_url', 'http://localhost:8080/fedora/risearch');
  $url .= "?type=tuples&flush=TRUE&format=csv&limit=1000&lang=itql&stream=on&query=";
  $content = do_curl($url . htmlentities(urlencode($query)));
  $list = explode("\n", $content);
  array_shift($list);
  foreach ($namespaces as $namespace) {
    $trimmed_names[] = trim($namespace);
  }
  $list = preg_replace('/info:fedora\//', '', $list);
  $options = array();
  foreach ($list as $item) {   //removes  blanks
    if ($item) {
      $parts = explode(',', $item);
      $namespace = explode(':', $parts[0]);
      $namespace = trim($namespace[0]);
      $bool = in_array($namespace, $trimmed_names);
      if (!$restricted || in_array($namespace, $trimmed_names)) {
        $options[$parts[0]] = $parts[1];
      }
    }
  }
  unset($options['islandora:ContentModelCollection']);
  return $options;
}

/**
 * Function: oai_get_content_models_as_array
 *
 * Description: Returns an associative array of all collection objects in Fedora instance
 *
 * @return array
 */
function oai_get_content_models_as_array() {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
  $query = 'select $object $title from <#ri>
          where ($object <dc:title> $title
          and ($object <fedora-model:hasModel> <info:fedora/fedora-system:ContentModel-3.0>
          or $object <fedora-rels-ext:isMemberOfCollection> <info:fedora/islandora:ContentModelsCollection>)
          and $object <fedora-model:state> <info:fedora/fedora-system:def/model#Active>)
          order by $title';

  $url = variable_get('fedora_repository_url', 'http://localhost:8080/fedora/risearch');
  $url .= "?type=tuples&flush=TRUE&format=csv&limit=1000&lang=itql&stream=on&query=";
  $content = do_curl($url . htmlentities(urlencode($query)));
  $list = explode("\n", $content);
  array_shift($list);
  $list = preg_replace('/info:fedora\//', '', $list);
  foreach ($list as $item) {   //removes  blanks
    if ($item) {
      $parts = explode(',', $item);
      $item = new fedora_item($parts[0]);
      if ($item->get_datastream_dissemination('ISLANDORACM')) {
        $options[$parts[0]] = $parts[1];
      }
    }
  }

  return $options;
}


/**
 * Function: build_oai_objects
 *
 * Description: Builds and ingests Fedora objects from OAI inputs
 *
 * @param array $record
 * @param string $namespace
 * @param string $collection_pid
 */
function build_oai_objects($record, $namespace, $collection_pid, $content_model) {
  set_time_limit(0);
  module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $thumbnail_file = drupal_get_path('module', 'islandora_harvester') . '/images/Crystal_Clear_action_filenew.png';

  $pid = fedora_item::get_next_PID_in_namespace($namespace);
  $dcStream = $record['dc'];
  $modsStream = oai_create_mods($dcStream);
  preg_match('/<dc:title>.*<\/dc:title>/', $dcStream, $matches);
  $title = preg_replace('/<\/?dc:title>/', "", $matches[0]);
  $item = fedora_item::ingest_new_item($pid, 'A', $title, 'Owner');
  $item->modify_datastream_by_value($dcStream, 'DC', "Dublin Core", 'text/xml');
  $item->add_datastream_from_string($modsStream, 'MODS', "MODS", 'text/xml');
  $item->add_relationship('isMemberOfCollection', $collection_pid, 'info:fedora/fedora-system:def/relations-external#');
  $item->add_relationship('hasModel', $content_model, 'info:fedora/fedora-system:def/model#');
  $item->add_datastream_from_file($thumbnail_file, 'TN');
}

/**
 * Function: build_csv_objects from CSV inputs
 *
 * Description: Builds and ingests Fedora objects
 *
 * @param array $headers
 * @param array $data
 * @param string $namespace
 * @param string $collection_pid
 */
function build_csv_objects($headers, $data, $namespace, $collection_pid, $content_model) {
  set_time_limit(0);
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'api/dublin_core');
  $thumbnail_file = drupal_get_path('module', 'islandora_harvester') . '/images/Crystal_Clear_action_filenew.png';
  $dc = new Dublin_Core();
  $limit = count($headers);
  for ($count = 0; $count < $limit; $count++) {
    $header = strtolower($headers[$count]);
    if (!preg_match('/^dc/', $header)) {
      $header = 'dc:' . $header;
    }
    if (preg_match('/dc\./', $header) && $data[$count]) {
      $header = str_replace('dc.', 'dc:', $header);
    }
    $dc->add_element($header, $data[$count]);
    if (!$title && $header == "dc:title") {
      $title = $data[$count];
    }
  }
  $pid = fedora_item::get_next_PID_in_namespace($namespace);
  $item = fedora_item::ingest_new_item($pid, 'A', $title, 'Owner');
  $item->add_relationship('isMemberOfCollection', $collection_pid, 'info:fedora/fedora-system:def/relations-external#');
  $item->add_relationship('hasModel', $content_model, 'info:fedora/fedora-system:def/model#');
  $item->modify_datastream_by_value($dc->as_xml(), 'DC', 'Dublin Core Metadata', 'text/xml', 'X');
  $modsStream = oai_create_mods($dc->as_xml());
  $item->add_datastream_from_string($modsStream, 'MODS', "MODS", 'text/xml');
  $item->add_datastream_from_file($thumbnail_file, 'TN');
}

/**
 * Function create_oai_batch
 *
 * Description: builds and kicks off oai batch
 *
 * @param string $namespace
 * @param string $collection_pid
 * @param array $urls
 */
function create_oai_batch($fullDom, $namespace, $collection_pid, $content_model) {

  $batch = array(
    'title' => "Creating objects in Fedora repository",
    'progress_message' => t('Processed @current objects out of @total.'),
    'operations' => array(),
    'file' => drupal_get_path('module', 'islandora_harvester') . '/islandora_harvester.inc',
  );
  $records_to_add = oai_get_input_data($fullDom);
  $count = count($records_to_add);
  foreach ($records_to_add as $record) {
    $batch['operations'][] = array('build_oai_objects', array($record, $namespace, $collection_pid, $content_model));
  }
  batch_set($batch);
}

/**
 * Function create_csv_batch
 *
 * Description: builds and kicks off csv batch
 *
 * @param File $file
 * @param string $namespace
 * @param string $collection_pid
 */
function create_csv_batch($file, $namespace, $collection_pid, $type, $content_model) {
  set_time_limit(0);
  if ($type == 'TSV') {
    $delimiter = "\t";
  }
  else {
    $delimiter = ",";
  }


  $csv = $file->filepath;
  $handle = fopen($csv, "r");
  $count = 1;
  $batch = array(
    'title' => t('Creating and ingesting Fedora objects'),
    'operations' => array(),
    'file' => drupal_get_path('module', 'islandora_harvester') . '/islandora_harvester.inc',
  );
  while (($data = fgetcsv($handle, 5000, $delimiter)) !== FALSE) {
    if ($count == 1) {
      foreach ($data as $header) {
        $header = preg_replace('/\[.*\]/', '', $header);
        if ($pos = strpos($header, '.', 4)) {
          $header = substr($header, 0, $pos);
        }
        $headers[] = $header;
      }
    }
    else {
      $batch['operations'][] = array('build_csv_objects', array($headers, $data, $namespace, $collection_pid, $content_model));
    }
    $count++;
  }
  batch_set($batch);

  return;
}



/**
 * Function build_full_dom
 *
 * Description: returns dom object with all objects to be imported
 * @param URL $url
 * @return DomDocument
 */
function build_full_dom($url) {
  set_time_limit(0);
  $urlParts = explode('?', $url);
  $base_url = $urlParts[0] . "?verb=ListRecords&resumptionToken=";
  $data = file_get_contents($url);
  if (!$data) {
    drupal_set_message("No results returned - please check url and login credentials", 'warning');
    return;
  }

  $dom = new DOMDocument();
  $dom->loadXML($data);
  $domBase = $dom->getElementsByTagName('ListRecords')->item(0);
  $resumption_node = $dom->getElementsByTagName('resumptionToken')->item(0);
  $token = $resumption_node->nodeValue;
  while ($token) {
    $new_url = $base_url . $token;
    $data = file_get_contents($new_url);
    $sub_dom = new DOMDocument();
    $sub_dom->loadXML($data);
    $records = $sub_dom->getElementsByTagName("record");
    foreach ($records as $record) {
      $newNode = $dom->importNode($record, TRUE);
      $domBase->appendChild($newNode);
    }
    $resumption_node = $sub_dom->getElementsByTagName('resumptionToken')->item(0);
    $token = $resumption_node->nodeValue;
  }
  $records = $dom->getElementsByTagName("record");
  return $dom;
}

/**
 * Function oai_create_mods
 *
 * Description: Creates MODS stream DC stream
 * @param string $xmlstr
 * @return string
 */
function oai_create_mods($xmlstr) {
  $path = drupal_get_path('module', 'islandora_harvester');
  module_load_include('inc', 'fedora_repository', 'api/ObjectHelper');
  module_load_include('inc', 'fedora_repository', 'core/CollectionClass');

  if ($xmlstr == NULL || strlen($xmlstr) < 5) {
    return " ";
  }

  try {
    $proc = new XsltProcessor();
  } catch (Exception $e) {
    drupal_set_message(t('@e', array('@e' => check_plain($e->getMessage()))), 'error');
    return " ";
  }

  $xsl = new DomDocument();
  $xsl->load($path . '/xslt/dc_to_mods.xsl');
  $input = new DomDocument();
  $input->loadXML(trim($xmlstr));
  $xsl = $proc->importStylesheet($xsl);
  $newdom = $proc->transformToDoc($input);
  $content = $newdom->saveXML();

  return $content;
}



function get_allowed_content_models($collection_pid) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');

  $collection_item = new fedora_item($collection_pid);
  $collection_policy = $collection_item->get_datastream_dissemination('COLLECTION_POLICY');
  $xml = simplexml_load_string($collection_policy);
  $json = json_encode($xml);
  $results = json_decode($json, true);
  if ($results) {
    $results_array = array();
    foreach ($results['content_models']['content_model'] as $cm) {
      $key = $cm['@attributes']['pid'];
      if (!$key) {
        $key = $cm['pid'];
      }
      $value = $cm['@attributes']['name'];
      if (!$value) {
        $value = $cm['name'];
      }
      $results_array[$key] = $value;
    }
    unset($results_array['islandora:collectionCModel']);
  }
  return $results_array;
}


function oai_get_input_data($dom){
  set_time_limit(0);
  module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
  $allData = array();
  $records = $dom->getElementsByTagName("record");

  foreach ($records as $record) {
    $recordData = array();
    $metadata = $record->getElementsByTagName('metadata')->item(0);
    $oai_dc = $metadata->getElementsByTagNameNS('http://www.openarchives.org/OAI/2.0/oai_dc/', '*');
    $oai_dc = $oai_dc->item(0);
    $recordData['dc'] = $dom->saveXML($oai_dc);
    $header = $record->getElementsByTagName('header')->item(0);
    $recordData['header'] = $dom->saveXML($header);
    $allData[] = $recordData;
  }
  return $allData;
}