import { InMemoryDbService } from 'angular2-in-memory-web-api';

/**
 * This class is only used as a mock for testing the services
 */
export class InMemoryDataService implements InMemoryDbService {
  createDb() {
    let aqstate =
      {
        "maxmq7":"273.00",
        "maxhumidity":"66.00",
        "maxtemp":"26.00",
        "maxdensity":"526.71",
        "minmq135":"22.00",
        "minmq131":"33.00",
        "avghumidity":"35.40",
        "avgmq131":"66.57",
        "maxmq131":"118.00",
        "avgdensity":"131.90",
        "mindensity":"-15.33",
        "avgmq7":"190.41",
        "minhumidity":"16.00",
        "avgmq135":"27.54",
        "minmq7":"105.00",
        "avgtemp":"15.58",
        "maxmq135":"67.00",
        "mintemp":"7.00",
        "status":"green"
      };
    return {aqstate};
  }
}