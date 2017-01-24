var markers = {
    overall: [],
    carbonmonoxide: [],
    ozone: [],
    organics: [],
    particle: [],
    temp: [],
    humidity: []
}

var icons = {
    green: {
        icon: 'img/green-marker.png'
    },
    yellow: {
        icon: 'img/yellow-marker.png'
    },
    orange: {
        icon: 'img/orange-marker.png'
    },
    red: {
        icon: 'img/red-marker.png'
    }
};

var infowindow;

function initMap() {

    map = new google.maps.Map(document.getElementById('map'), {
        zoom: 12,
        center: new google.maps.LatLng(33.521236, -86.808944),
        mapTypeId: 'roadmap'
    });

    infowindow = new google.maps.InfoWindow();

    // add listeners for zoom, center and bounds changes
    map.addListener('center_changed', reloadData);
    map.addListener('zoom_changed', reloadData);
    map.addListener('bounds_changed', reloadData);

    // get the viewport coordinates so we an ask Elasticsearch for the right data
    var readings = getReadings(map.getBounds());

    // clear the old markers

    // add the new markers
    for (var i = 0, reading; reading = readings[i]; i++) {
        addMarker(reading);
    }
}

function reloadData(event) {

}

function addMarker(feature, zoomlevel) {

    var contentString = '<div id="content">' +
        '<h4>Location Reading</h4>' +
        '<div id="bodyContent">' +
        '<ul>' +
        '<li>Time Taken:</li>' +
        '<li>O3:  CO:  General:</li>' +
        '<li>PM:</li>' +
        '<li>Temperature: Humidity:</li>' +
        '</ul>' +
        '</div>' +
        '</div>';

    // set the size according to the zoom level
    var aqicon = {
        url: icons[feature.type].icon,
        scaledSize: new google.maps.Size(16, 16)
    };

    var marker = new google.maps.Marker({
        position: feature.position,
        icon: aqicon,
        map: map
    });

    marker.addListener('click', function () {
        infowindow.setContent(contentString);
        infowindow.open(map, marker);
    });
}

function getReadings(latlngbounds) {

    // a reading represents a location, a time, 
    // and all of the sensor readings and server
    // side calculated values.  Each reading gets

    var readings = [
        {
            position: new google.maps.LatLng(33.52, -86.8),
            time: new Date(),
            type: 'green'
        }, {
            position: new google.maps.LatLng(33.51213, -86.7543454),
            type: 'green'
        }, {
            position: new google.maps.LatLng(33.523455, -86.78456),
            type: 'yellow'
        }, {
            position: new google.maps.LatLng(33.5266546, -86.7245545),
            type: 'red'
        }
    ];

    return readings;
}