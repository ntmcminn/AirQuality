function initMap() {
   
    map = new google.maps.Map(document.getElementById('map'), {
        zoom: 12,
        center: new google.maps.LatLng(33.521236, -86.808944),
        mapTypeId: 'roadmap'
    });

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

    function addMarker(feature) {

        var contentString = '<div id="content">' +
            '<h4>Location Reading</h4>' +
            '<div id="bodyContent">' +
            '<ul>' +
            '<li>O3:  CO:  General:</li>' +
            '<li>PM:</li>' +
            '<li>Temperature: Humidity:</li>' +
            '</ul>' +
            '</div>' +
            '</div>';

        var infowindow = new google.maps.InfoWindow({
            content: contentString
        }); 
        
        var aqicon = {
            url: icons[feature.type].icon,
            size: new google.maps.Size(64, 64)
        };

        var marker = new google.maps.Marker({
            position: feature.position,
            icon: aqicon,
            map: map
        });

        marker.addListener('click', function() {
            infowindow.open(map, marker);
        });
    }

    var features = [
        {
            position: new google.maps.LatLng(33.52, -86.8),
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

    for (var i = 0, feature; feature = features[i]; i++) {
        addMarker(feature);
    }
}