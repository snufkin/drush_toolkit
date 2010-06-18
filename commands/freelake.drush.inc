<?php

function freelake_drush_help($section) {
  switch ($secion) {
    case 'drush:reset':
      return dt('Wipe the database.');
    case 'drush:templates':
      return dt("Usage: drush templates. List top level template suggestions.");
    case 'drush:preprocess':
      return dt("Usage: drush preprocesses. List preprocess names for the templates.");
    case 'drush:fields':
      return dt("Usage: drush fields. List fields available in views.");
    case 'drush:pw-reset':
      return dt("Usage: drush pw-reset [username] [password]. Reset the password for a given user.");
  }
}

function freelake_drush_command() {
  // This is copied from the sql command pack.
  $options['--database'] = 'The DB connection key if using multiple connections in settings.php.';
  if (drush_drupal_major_version() >= 7) {
    $options['--target'] = 'The name of a target within the specified database.';
  }

  $items['reset'] = array(
    'description' => 'Reset the database, dropping all the tables.',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_DATABASE,
    'options' => $options,
  );
  $items['templates'] = array(
    'callback' => 'drush_freelake_templates',
    'description' => 'List top-level template names for view displays.',
    'drupal dependencies' => array('views'),
  );
  $items['preprocess'] = array(
    'callback' => 'drush_freelake_preprocess',
    'description' => 'Theme preprocess function names that need to be implemented.',
    'drupal dependencies' => array('views'),
  );
  $items['fields'] = array(
    'callback' => 'drush_freelake_fields',
    'description' => 'List fields available in views.',
    'drupal dependencies' => array('views'),
  );
  $items['pw-reset'] = array(
    'callback' => 'drush_freelake_pw_reset',
    'description' => 'Change/reset the password for a user. Usage: drush pw-reset [username/uid] [password]. If no arguments given username will assume user 1 and the password will be drupal.',
  );
  return $items;
}

function drush_freelake_reset($db_spec = NULL) {
  $db = _drush_sql_get_db_spec();
  drush_sql_query('DROP DATABASE '. $db['database'], $db);
  drush_sql_query('CREATE DATABASE '. $db['database'], $db);
}

function drush_freelake_templates() {
  $views = func_get_args();
  foreach ($views as $view_id) {
    $view = views_get_view($view_id);
    foreach ($view->display as $display_id => $display) {
      // Ignore displays that are default, or feed.
      if ('default' == $display_id || 'feed_1' == $display_id) {
        continue;
      }
      $theme = variable_get('theme_default', 'garland');
      $themes = views_theme_functions($theme, $view, $display);
      $theme_name = $themes[0];
      $name = str_replace($theme, 'views-view-fields', $theme_name);
      $name = str_replace('_', '-', $name);
    
      echo $name . ".tpl.php\n";
    }
  }
}

function drush_freelake_fields($view_name, $display_id = 'default') {
  $view = views_get_view($view_name);
  $display = $view->display[$display_id];

  if (empty($display->display_options['fields'])) {
    $display = $view->display['default'];
  }
  $fields = array_keys($display->display_options['fields']);

  foreach ($fields as $field) {
    echo "<?php print \$fields['$field']->content; ?>\n";
  }
}

function drush_freelake_preprocess() {
  $views = func_get_args();
  foreach ($views as $view_id) {
    $view = views_get_view($view_id);
    foreach ($view->display as $display_id => $display) {
      $theme = variable_get('theme_default', 'garland');
      $functions = views_theme_functions($theme, $view, $display);
      $function = $functions[0];
      $name = str_replace($theme, $theme . '_preprocess_', $function);
      echo "function " . $name .'(&$variables) {' . "\n\n}\n";
    }
  }
}

/**
 * Reset the password for a given user (by default it will be the root user).
 */
function drush_freelake_pw_reset($user = 1, $password = 'drupal') {
  if (is_numeric($user)) {
    $account = user_load(array('uid' => $user));
  }
  else {
    $account = user_load(array('name' => $user));
  }
  
  if ($account->uid == 1) {
    $data['name'] = 'admin';
  }
  $data['pass'] = $password;
  // user_save takes care of generating the md5 hash.
  user_save($account, $data);
}