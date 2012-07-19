<?php

function oai_ingest_form(&$form_state, $pid) {

  module_load_include('inc', 'islandora_harvester', 'islandora_harvester');
  $path = drupal_get_path('module', 'islandora_harvester');
  drupal_add_js("$path/js/fix-jquery-update.js", 'theme');
  $allowed_content_models = get_allowed_content_models($pid);
  $input_type = $form_state['values']['input_type'];
  $chosen_model = $form_state['values']['existing_collection'];
  $form['#attributes']['enctype'] = 'multipart/form-data';

  $form['link'] = array(
    '#type' => 'item',
    '#value' => l(t("Return to collection view"), "fedora/repository/$pid"),
  );

  $form['source_data'] = array(
    '#prefix' => '<div id="source-info">',
    '#suffix' => '</div>',
    '#type' => 'fieldset',
    '#title' => t('Source Data')
  );

  $form['source_data']['input_type'] = array(
    '#type' => 'radios',
    '#title' => t('Source type'),
    '#options' => array('CSV' => 'CSV', 'TSV' => 'TSV', 'OAI' => 'OAI-DC'),
    '#default_value' => $input_type,
    '#ahah' => array(
      'path' => 'oai/source/update/source_data',
      'wrapper' => 'source-info',
      'effect' => 'fade',
      'event' => 'change'),
  );

  if ($input_type == 'CSV' || $input_type == 'TSV') {
    $form['source_data']['file_upload'] = array(
      '#title' => 'Upload document',
      '#type' => 'file',
      '#description' => t("Upload $input_type source file for processing"),
    );
  }

  if ($input_type == 'OAI') {
    $form['source_data']['url'] = array(
      '#type' => 'textfield',
      '#title' => "Harvest from an external repository (OAI Set)",
      '#maxlength' => 256,
      "#description" => t('Enter full url to retrieve entire OAI resultset.'),
    );
  }

  $form['collection'] = array(
    '#type' => 'hidden',
    '#value' => $pid,
  );

  if ($allowed_content_models) {
    $form['content_model'] = array(
      '#title' => "Choose content model",
      '#type' => 'select',
      '#options' => $allowed_content_models,
      '#description' => t("Content models describe the behaviours of objects with which they are associated."),
    );
  }
  else {
    drupal_set_message(t('This collection has no content models in its Collection Policy'));
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit')
  );


  return($form);
}

function oai_ingest_form_submit($form, &$form_state) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'islandora_harvester', 'islandora_harvester');
  $collection_pid = $form_state['values']['collection'];
  $content_model = $form_state['values']['content_model'];
  $collection_item = new fedora_item($collection_pid);
  $collection_policy = $collection_item->get_datastream_dissemination('COLLECTION_POLICY');
  $xml = simplexml_load_string($collection_policy);

  $json = json_encode($xml);
  $results = json_decode($json, true);

  $namespace = $collection_pid;
  foreach ($results['content_models']['content_model'] as $cm) {
    if ($cm['@attributes']['pid'] == $content_model) {
      $namespace = $cm['@attributes']['namespace'];
    }
  }
  $namespace = substr($namespace, 0, strpos($namespace, ":"));

  if (!empty($form_state['ahah_submission'])) {
    return;
  }

  if ($form_state['values']['url']) {
    $url = $form_state['values']['url'];
    $urlParts = explode("?", $url);
    $root_url = $urlParts[0];
    $fullDom = build_full_dom($url);
    create_oai_batch($fullDom, $namespace, $collection_pid, $content_model);
    return;
  }

  if ($_FILES['files']['name']['file_upload']) {
    $csv_file = file_save_upload('file_upload');
    create_csv_batch($csv_file, $namespace, $collection_pid, $form_state['values']['input_type'], $form_state['values']['content_model']);
    return;
  }
}

function update_source_div($data_source) {
  $form = oai_callback_prep();
  $changed_elements = $form[$data_source];
  unset($changed_elements['#prefix'], $changed_elements['#suffix']);
  $output = theme('status_messages') . drupal_render($changed_elements);
  drupal_json(array(
    'status' => TRUE,
    'data' => $output,
  ));
}

function update_destination_div($data_source) {
  $form = oai_callback_prep();
  $changed_elements = $form[$data_source];
  unset($changed_elements['#prefix'], $changed_elements['#suffix']);
  $output = theme('status_messages') . drupal_render($changed_elements);
  drupal_json(array(
    'status' => TRUE,
    'data' => $output,
  ));
}

function oai_callback_prep() {
  $form_state = array('storage' => NULL, 'submitted' => FALSE);
  $form_build_id = $_POST['form_build_id'];
  $form = form_get_cache($form_build_id, $form_state);
  $args = $form['#parameters'];
  $form_id = array_shift($args);
  $form_state['post'] = $form['#post'] = $_POST;
  // Enable the submit/validate handlers to determine whether AHAH-submittted.
  $form_state['ahah_submission'] = TRUE;
  $form['#programmed'] = $form['#redirect'] = FALSE;
  drupal_process_form($form_id, $form, $form_state);
  $form = drupal_rebuild_form($form_id, $form_state, $args, $form_build_id);
  return $form;
}