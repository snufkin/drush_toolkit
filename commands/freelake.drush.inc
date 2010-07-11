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
    case 'drush:variable-list':
      return dt("List, or export Drupal variables. Without any arguments a list of all variables will be shown. If arguments are specified they'll be var_exported to stdout.");
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
  $items['variable-list'] = array(
    'callback' => 'drush_freelake_variable_list',
    'description' => 'Export variable(s).',
    'aliases' => array('vlist'),
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

/**
 * List the variable names used.
 */
function drush_freelake_variable_list() {
  $vars = func_get_args();
  if (empty($vars)) {
    // Pull all variables from DB.
    $result = db_query('SELECT name FROM variable');
    $variables = array();
    while ($variable = db_fetch_object($result)) {
      $variables[$variable->name] = $variable->name;
    }

    // Pull all variables defined in hook_strongarm(), merge them in.
    $strongarmed = drupal_map_assoc(array_keys(strongarm_get_conf(TRUE, TRUE)));
    $variables = $variables + $strongarmed;
    ksort($variables);

    // Output result, flag strongarmed variables.
    $rows = array(array('Strongarmed', 'Variable name'));
    foreach ($variables as $variable) {
      $flag = $strongarmed[$variable] ? '*' : '';
      $rows[] = array($flag, $variable);
    }
    drush_print_table($rows, TRUE);
  }
  else {
    $exports = array();
    foreach ($vars as $v) {
      // We use the very unlikely 'DRUSH_STRONGARM_NO_VALUE_HERE' as the 
      // default value which *should* be ok, right?
      $variable = variable_get($v, 'DRUSH_STRONGARM_NO_VALUE_HERE');
      if ($variable !== 'DRUSH_STRONGARM_NO_VALUE_HERE') {
        $exports[$v] = $variable;
      }
    }
    drush_print(var_export($exports));
  }
}

/**
 * Backporting the strongarm_get_conf. TODO rewrite above command to use strongarm code.
 */
if (!function_exists('strongarm_get_conf')) {
  /**
   * Retrieve variable configuration from the cache.
   */
  function strongarm_get_conf($bootstrapped = TRUE, $reset = FALSE) {
    static $var_conf = array();
    if (!isset($var_conf) || $reset) {
      if (!$reset) {
        $cache = cache_get('strongarm', 'cache');
        $var_conf = $cache ? $cache->data : NULL;
      }
      if (!isset($var_conf) && $bootstrapped) {
        // We don't use module_invoke_all() here since
        // array_merge_recursive() can alter the desired structure of
        // some variables.
        foreach (module_implements('strongarm') as $module) {
          $module_conf = module_invoke($module, 'strongarm');
          foreach ($module_conf as $name => $value) {
            if (isset($var_conf[$name])) {
              if (is_array($value)) {
                $var_conf[$name] = array_merge($var_conf[$name], $value);
              }
              else {
                // Blow the earlier one away
                $var_conf[$name] = $value;
              }
            }
            else {
              $var_conf[$name] = $value;
            }
          }
        }
        cache_set('strongarm', $var_conf);
      }
    }
    return $var_conf;
  }
}