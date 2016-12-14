var aqstatus = angular.module('aqMonitor', []);

function mainController($scope, $http) {
    $scope.formData = {};

    // when landing on the page, get all todos and show them
    $http.get('/api/aqstate')
        .success(function(data) {
            $scope.status = data;
            console.log(data);
        })
        .error(function(data) {
            console.log('Error: ' + data);
        });

}