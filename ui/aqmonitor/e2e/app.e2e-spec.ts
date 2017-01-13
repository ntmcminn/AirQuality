import { AqmonitorPage } from './app.po';

describe('aqmonitor App', function() {
  let page: AqmonitorPage;

  beforeEach(() => {
    page = new AqmonitorPage();
  });

  it('should display message saying app works', () => {
    page.navigateTo();
    expect(page.getParagraphText()).toEqual('app works!');
  });
});
