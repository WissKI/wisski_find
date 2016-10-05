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
  
  $search = (array) $search;

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

  $operators = array(
    'smaller' => '<',
    'smallerequal' => '<=',
    'bigger' => '>',
    'biggerequal' => '>=',
    'exact' => 1,
    'like' => 1,
    'regex' => 1,
    'not_empty' => 1,
  );
  if (!isset($operators[$match_op])) {
    drupal_set_message("Invalid matching mode: '$match_op'", 'error');
    return array();
  }

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
  } else {
    $numeric_term = is_numeric(trim($term));
    $term = $numeric_term ? trim($term) : ('"'.   str_replace('"', '\\"', $term) . '"');
    $var = $numeric_term ? 'xsd:integer(?out)' : 'str(?out)';
    $op = $operators[$match_op];
    $q = wisski_pathbuilder_get_sparql($pathid, "FILTER ( $var $op $term ) ", FALSE);
  }

  /*
  $orderpath = variable_get("wisski_order_path", array());
  
  $groupid = arg(1);
  
  $orderpath = $orderpath[$groupid];
  
  if(!empty($orderpath) && !is_array($orderpath)) {
    $orderpath = array($orderpath);
  }
  
  
  if(!empty($orderpath)) {
    $sparql = "";
    $orderby = " ORDER BY ";
    
    foreach($orderpath as $currpathid => $currpath) {
      
      if(empty($currpath))
        continue;
      
      // get the path for the order by
      $sparqlorderparts = wisski_pathbuilder_get_sparql($currpath, "", TRUE);
      
      $sparqlorder = "";
      
      foreach($sparqlorderparts["triples"] as $triple) {
        $sparqlorder .= $triple . " . ";
      }
      
      $sparqlorder = str_replace("?x", "?u" . $currpathid . '_', $sparqlorder);
      $sparqlorder = str_replace("?out", "?orderout" . $currpathid, $sparqlorder);
      $sparqlorder = str_replace("?u" . $currpathid . "_0", "?x0", $sparqlorder);
                              
      $sparql .= " . OPTIONAL { " . $sparqlorder . " } . ";
    
      if(variable_get("wisski_navigator_alphanumeric_sort", 1)) {
        $orderby .= ("DESC(BOUND(?orderout" . $currpathid . ")) ASC(xsd:string(?orderout" . $currpathid . ")) ");
      } else {
        $orderby .= ("?orderout" . $currpathid . " ");
      }
    }
  }
  
  $q = substr($q, 0, -4) . $sparql . '} . ' . $orderby;

  drupal_set_message(htmlentities($q));
  
  return "";
*/
  $rows = $store->query($q, 'rows');

  if ($store->getErrors()) {
    drupal_set_message("errors processing query for path $pathid, query:<br>" . str_replace("\n", "<br>", check_plain($q)) . "<br><ul><li>" . implode('</li><li>',$store->getErrors()) . "</li></ul>", 'error');
    return array();
  }

  $ret = array();
  foreach ($rows as $r) {
    if (!isset($ret[$r['x0']])) $ret[$r['x0']] = array();
    $ret[$r['x0']][] = array('path' => $pathid, 'val' => $r['out']);
  }

  return $ret;
  
}


