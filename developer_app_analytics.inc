<?php

function developer_app_analytics_performance($user, $app_id, $app_name = NULL) {
  module_load_include('inc', 'devconnect_developer_apps', 'analytics');
  $entities = entity_load('developer_app', array($app_id));
  $entity = reset($entities);
  $app_name = $entity->name;
  $my_data = _apigee_developer_app_analytics_get_datapoints($user, $app_name);
  $my_error_data = _apigee_developer_app_analytics_get_datapoints($user, $app_name, 'error_count');
  $my_trt_data = _apigee_developer_app_analytics_get_datapoints($user, $app_name, 'total_response_time');
  if ($my_data === FALSE) {
      return '<p>' . t('An error occurred while fetching your data.') . '</p>';
  }

  _developer_app_analytics_insert_chart_js($my_data, 'analytics-traffic-highchart', $app_name);
  _developer_app_analytics_insert_chart_js($my_error_data, 'analytics-error-highchart', $app_name, '#ff4503', '#f9d5c8');
  _developer_app_analytics_insert_chart_js($my_trt_data, 'analytics-total-response-time-highchart', $app_name);


  if (variable_get('devconnect_highcharts_use_cdn', 'cdn') == 'cdn') {
        $highcharts_base_url = '//code.highcharts.com/3.0.10';
        $type = 'external';
  }
  else {
        $highcharts_base_url = url(libraries_get_path('highcharts'));
        $type = 'file';
  }

  drupal_add_js($highcharts_base_url . '/highcharts.js', array('type' => $type, 'group' => JS_LIBRARY));


  // TODO: FIXME eliminate inline style.
  return '<div id="analytics-traffic-highchart" style="min-width:1080px; height: 250px; margin: 0 auto"></div>
          <div id="analytics-error-highchart" style="min-width:1080px; height: 250px; margin: 0 auto"></div>
          <div id="analytics-total-response-time-highchart" style="min-width:1080px; height: 250px; margin: 0 auto"></div>';

}


function _apigee_developer_app_analytics_get_datapoints($user, $app_name, $metric = 'message_count', $params = array()) {
    $dca = _devconnect_developer_apps_analytics_api_client();

    $metrics = _apigee_developer_app_analytics_metrics();
    $units = array('second', 'minute', 'hour', 'day', 'week', 'month','year');

    if (isset($_GET['metric']) && isset($metrics[$_GET['metric']])) {
        $metric = $_GET['metric'];
    }
    else if(isset($params['metric']) && isset($metrics[$params['metric']])) {
        $metric = $params['metric'];
    }

    if (isset($_GET['to'])) {
        $end = $_GET['to'];
    }
    else if(isset($params['to'])) {
        $end = $params['to'];
    }
    else {
        $end = time();
    }

    if (isset($_GET['from']) && $_GET['from'] < $end) {
        $start = $_GET['from'];
    }
    else if(isset($params['from'])) {
        $start = $params['from'];
    }
    else {
        $start = strtotime('-24 hours', $end);
    }

    if (isset($_GET['unit']) && in_array($_GET['unit'], $units)) {
        $unit = $_GET['unit'];
    }
    else if(isset($params['unit'])) {
        $unit = $params['unit'];
    }
    else {
        $unit = 'hour';
    }
    $order = 'ASC';

    // 4G accepts UTC dates.
    $end = strtotime(gmdate('Y-m-d H:i:s', $end));
    $start = strtotime(gmdate('Y-m-d H:i:s', $start));

    $time_points = array();
    switch ($unit) {
        case 'second':
            $date_format = 'H:i:s';
            $current_time = $start;
            while ($current_time <= $end) {
                $time_points[] = $current_time;
                $current_time++;
            }
            break;
        case 'minute':
            $date_format = 'H:i';
            $current_time = $start - ($start % 60);
            while ($current_time <= $end) {
                $time_points[] = $current_time;
                $current_time += 60;
            }
            break;
        case 'hour':
            $date_format = 'H:00';
            $current_time = $start - ($start % 3600);
            while ($current_time <= $end) {
                $time_points[] = $current_time;
                $current_time += 3600;
            }
            break;
        case 'day':
            $date_format = 'j M';
            $current_time = $start - ($start % 86400);
            while ($current_time <= $end) {
                $time_points[] = $current_time;
                $current_time += 86400;
            }
            break;
        case 'week':
            $date_format = 'j M';
            $week_seconds = 86400 * 7;
            $current_time = $start - ($start % $week_seconds);
            while ($current_time <= $end) {
                $time_points[] = $current_time;
                $current_time += $week_seconds;
            }
            break;
        case 'month':
            $date_format = 'M Y';
            $current_time = mktime(0, 0, 0, date('m', $start), 1, date('Y', $start));
            while ($current_time <= $end) {
                $time_points[] = $current_time;
                $current_time = strtotime('+1 month', $current_time);
            }
            break;
        case 'year':
            $date_format = 'Y';
            $current_time = mktime(0, 0, 0, 1, 1, date('Y', $start));
            while ($current_time <= $end) {
                $time_points[] = $current_time;
                $current_time = strtotime('+1 year', $current_time);
            }
            break;
    }
    $default_datapoints = array();
    foreach ($time_points as $time_point) {
        $default_datapoints[$time_point] = 0;
    }

    // We will pass all the parameters that are required to fetch the analytics
    // data from the backend and that are required to render the data on the
    // graph as an array to drupal_alter() to be modified by other modules.
    $info['developer_id'] = devconnect_user_get_developer_id($user);
    $info['app_name'] = $app_name;
    $info['metric'] = $metric;
    $info['start'] = $start;
    $info['end'] = $end;
    $info['unit'] = $unit;
    $info['order'] = $order;
    $info['time_points'] = $time_points;
    $info['default_datapoints'] = $default_datapoints;
    $info['timezone'] = "UTC";

    // Pass the $info to drupal_alter to be modified by other modules.
    drupal_alter('devconnect_developer_app_analytics_datapoints_params', $info);

    try {
        $developer_id = devconnect_user_get_developer_id($user);
        // We pass the altered version of the parameters to the call to get the
        // analytics data.
        $datapoints = $dca->getByAppName($developer_id, $app_name, $info['metric'], $info['start'], $info['end'], $info['unit'], $info['metric'], $info['order']);
    } catch (Apigee\Exceptions\ParameterException $e) {
        watchdog('devconnect_developer_apps', $e->getMessage(), array(), WATCHDOG_ERROR);
        return FALSE;
    } catch (Exception $e) {
        watchdog('devconnect_developer_apps', $e->getMessage(), array(), WATCHDOG_ERROR);
        return FALSE;
    }

    // Allow modules to alter the datapoints.
    drupal_alter('devconnect_developer_app_analytics_datapoints', $app_name, $datapoints);

    // We return the altered version of the parameters so that it is reflected in
    // the graph and the downloads.
    return array(
        'date_format' => $date_format,
        'datapoints' => $datapoints,
        'default_datapoints' => $info['default_datapoints'],
        'time_points' => $info['time_points'],
        'params' => array(
            'metric' => array(
                'display_name' => $metrics[$metric],
                'name' => $metric
            ),
            'start' => $info['start'],
            'end' => $info['end'],
            'unit' => $info['unit'],
            'timezone' => $info['timezone'],
        ),
    );

}

function _developer_app_analytics_insert_chart_js($my_data, $id, $app_name, $color='#258ec7', $color_area = '#cce6f1') {
    $datapoints = $my_data['datapoints'];
    $date_format = $my_data['date_format'];
    $default_datapoints = $my_data['default_datapoints'];
    $time_points = $my_data['time_points'];

    $metric_fullname = $my_data['params']['metric']['display_name'];
    $metric_name = $my_data['params']['metric']['name'];

    if (count($datapoints) == 0 || !isset($datapoints[$app_name])) {
        // TODO: handle this differently?
        return '';
    }


    $chart_data = array(
        'chart' => array(
            'zoomType' => 'x',
            'renderTo' => $id,
            'marginRight' => 50, // If we suppress legend, this gets set to 0 below
            'type' => 'area',
            'marginBottom' => 50,
        ),
        'title' => array(
            'text' => $metric_fullname
        ),
        'xAxis' => array(
            'categories' => array(), // Filled in below
            'title' => array(
                'text' => 'Time (UTC)',
                'margin' => 0
            )
        ),
        'yAxis' => array(
            'title' => array('text' => $metric_fullname), // may be modified below
            'plotLines' => array(
                array(
                    'value' => 0,
                    'width' => 1,
                    'color' => '#808080'
                )
            ),
            'min' => 0
        ),
        'legend' => array(
            'layout' => 'vertical',
            'align' => 'right',
            'verticalAlign' => 'top',
            'x' => -10,
            'y' => 100,
            'borderWidth' => 0
        ),
        'series' => array(), // filled in below
        'credits' => array('enabled' => FALSE),
        'plotOptions' => array(
            'area' => array(
                'fillColor' => array(
                    'linearGradient' => array(
                        'x1' => 0,
                        'y2' => 0,
                        'x2' => 0,
                        'y2' => 1
                    ),
                    'stops' => array(
                        array(0, $color_area),
                        array(1, $color_area),
                    )
                ),
                'lineWidth' => 2,
                'marker' => array(
                    'radius' => 2
                ),
                'states' => array(
                    'hover' => array(
                        'lineWidth' => 1
                    )
                ),
                'threshold' => null
            )
        )
    );

    if (substr($metric_name, -5) == '_time') {
        $chart_data['yAxis']['title']['text'] = 'Milliseconds';
    }

    // Step across X-axis labels if there are too many.
    $step = 1;
    if (count($time_points) > DEVCONNECT_ANALYTICS_MAX_XAXIS_LINES) {
        $step = ceil(count($time_points) / DEVCONNECT_ANALYTICS_MAX_XAXIS_LINES);
        $chart_data['xAxis']['labels'] = array('step' => $step);
    }

    $app = $datapoints[$app_name];

    $last_time = 0;
    foreach ($time_points as $i => $time_point) {
        $x_caption = date($date_format, $time_point);
        // If granularity < day and date has changed, show date under time.
        if (substr($date_format, 0, 1) == 'H' && ($i % $step == 0) && ($last_time == 0 || date('d', $time_point) != date('d', $last_time))) {
            // PLEASE NOTE: Not sure, but I think inline styles are necessary here because they are being embedded in a Highchart
            $x_caption .= ' <div style="font-size:0.75em;font-style:italic;margin-top:-3px">' . date('F j', $time_point) . '</div>';
            $chart_data['xAxis']['labels']['useHTML'] = TRUE;
            $last_time = $time_point;
        }

        $chart_data['xAxis']['categories'][] = $x_caption;
    }

    // Read in all values for all environments.
    $data = array();
    foreach ($app as $env => $env_datapoints) {
        $data[$env] = $default_datapoints;
        foreach ($env_datapoints as $time_point => $value) {
            $data[$env][$time_point] = $value;
        }
    }

    if (count($data) == 1) {
        $chart_data['legend']['enabled'] = FALSE;
        $chart_data['chart']['marginRight'] = 0;
    }

    foreach ($data as $env => $time_points) {
        $chart_data['series'][] = array(
            'name' => $metric_fullname,
            'data' => array_values($time_points),
            'color' => $color
        );
    }

    // Allow other modules to alter the chart settings.
    drupal_alter('devconnect_developer_apps_analytics_chart_data', $chart_data);
    $chart_json = json_encode($chart_data);

    $script = "jQuery(document).ready(function(){chart=new Highcharts.Chart($chart_json);});";
    drupal_add_js($script, array('type' => 'inline'));
}

function _apigee_developer_app_analytics_metrics() {
    return array(
        'total_response_time' => t('Total Response Time'),
        'max_response_time' => t('Max Response Time'),
        'min_response_time' => t('Min Response Time'),
        'end_point_response_time' => t('Endpoint Response Time'),
        'message_count' => t('Traffic'),
        'error_count' => t('Errors'),
    );
}