<?php


/**
* $search is an array that represents the search structure
* array(
*   'op' => 'OR' | 'AND'
*   'args' => an array of arguments for the operator; an argument may be a search structure or a search term structure
*   
* )
* search term structure is
* array(
*   'path' => <path ID>
*   'term' => <search term>   // the string that shall be searched for
*   'match' => 'exact' | 'like' | 'regex' // how and how strict shall the term be applied 
* )
*/
function wisski_find_search($search, $store = NULL, $namespaces = array()) {

  if ($store === NULL) {
    drupal_load('module', 'wisski_store');
    $store = wisski_store_getObj()->wisski_ARCAdapter_getStore();
    $namespaces = wisski_store_getObj()->wisski_ARCAdapter_getNamespaces();
  }

  if (isset($search['op'])) {
    return _wisski_find_searchOP($search, $store, $namespaces);
  } else {
    return _wisski_find_searchTerm($search, $store, $namespaces);
  }
  
}
   

/** helper function for wisski_find_Search()
*process a search structure
* 
*/
function _wisski_find_searchOP($search, $store, $namespaces) {

  $op = $search['op'];
  $args = $search['args'];
  $l = count($args);
  
  if ($op == 'OR') {
    
    $ret = wisski_find_search($args[0], $store, $namespaces);

    for ($i = 1; $i < $l; $i++) {
      foreach (wisski_find_search($args[$i], $store, $namespaces) as $uri => $cnt) {
        if (isset($ret[$uri])) {
          $ret[$uri] = array_merge($ret[$uri], $cnt);
        } else {
          $ret[$uri] = $cnt;
        }
      }
    }

    return $ret;

  } elseif ($op == 'AND') {
    
    $ret = wisski_find_search($args[0], $store, $namespaces);
    for ($i = 1; $i < $l; $i++) {
      $ret1 = array();
      $nret = wisski_find_search($args[$i], $store, $namespaces);
      foreach($nret as $uri => $cnt) {
        if (isset($ret[$uri])) $ret1[$uri] = array_merge($ret[$uri], $cnt);
      }
      $ret = $ret1;
    }

    return $ret;

  } elseif ($op == 'NOT') {
    
    $ret = wisski_find_search($args[0], $store, $namespaces);
    for ($i = 1; $i < $l; $i++) {
      $ret1 = array();
      $nret = wisski_find_search($args[$i], $store, $namespaces);
      foreach($ret as $uri => $cnt) {
        if (!isset($nret[$uri])) $ret1[$uri] = $ret[$uri];
      }
      $ret = $ret1;
    }

    return $ret;

  }
 
}


/** helper function for wisski_find_search().
* process a search term structure
*/
function _wisski_find_searchTerm($search, $store, $namespaces) {
  $pathid = $search['path'];
  $term = $search['term'];
  $match_op = $search['match'];

  include_once(drupal_get_path('module', 'wisski_pathbuilder') . "/wisski_pathbuilder.inc");


  $path = wisski_pathbuilder_getPathData($pathid);

  $result = array();

  foreach ($namespaces as $pre => $ns) $q .= "PREFIX $pre: <$ns>\n";

  if ($match_op == 'exact') {
    $q = wisski_pathbuilder_get_sparql($pathid, 'FILTER ( ?out = "'.   str_replace('"', '\\"', $term) . '" ) ', FALSE);
  } elseif ($match_op == 'like') {
    $q = wisski_pathbuilder_get_sparql($pathid, 'FILTER regex(?out, "' . str_replace('\\', '\\\\', str_replace('"', '\\"', preg_quote($term)))  . '", "i") ', FALSE);
  } elseif ($match_op == 'regex') {
    $q = wisski_pathbuilder_get_sparql($pathid, 'FILTER regex(?out, "' . str_replace('"', '\\"', $term) . '", "i") ', FALSE);
  } elseif($match_op == 'not_empty') {
    $q = wisski_pathbuilder_get_sparql($pathid, '', FALSE);
  } elseif ($match_op == 'smaller') {
    $q = wisski_pathbuilder_get_sparql($pathid, 'FILTER ( ?out < "'.   str_replace('"', '\\"', $term) . '" ) ', FALSE);
  } elseif ($match_op == 'smallerequal') {
    $q = wisski_pathbuilder_get_sparql($pathid, 'FILTER ( ?out <= "'.   str_replace('"', '\\"', $term) . '" ) ', FALSE);
  } elseif ($match_op == 'bigger') {
    $q = wisski_pathbuilder_get_sparql($pathid, 'FILTER ( ?out > "'.   str_replace('"', '\\"', $term) . '" ) ', FALSE);
  } elseif ($match_op == 'biggerequal') {
    $q = wisski_pathbuilder_get_sparql($pathid, 'FILTER ( ?out >= "'.   str_replace('"', '\\"', $term) . '" ) ', FALSE);
  } else {
    drupal_set_message("Invalid matching mode: '$match_op'", 'error');
  }

  $rows = $store->query($q, 'rows');

  if ($store->getErrors()) {
    drupal_set_message("errors processing query for path $pathid, query:<br>" . str_replace("\n", "<br>", check_plain($q)) . "<br><ul><li>" . implode('</li><li>',$store->getErrors()) . "</li></ul>", 'error');
    return array();
  }

  $ret = array();
  foreach ($rows as $r) {
    $ret[$r['x0']] = array(array('path' => $pathid, 'val' => $r['out']));
  }

  return $ret;
  
}


