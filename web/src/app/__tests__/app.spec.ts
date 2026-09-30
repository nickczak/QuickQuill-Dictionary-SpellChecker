import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { of } from 'rxjs';
import { App } from '../app';
import { Auth } from '../services/auth';
import { Wotd } from '../services/wotd';
import { provideApiStub } from '../testing/api-stubs';
import { clearStoredSession } from '../testing/auth-stub';

describe('App startup', () => {
  beforeEach(() => {
    clearStoredSession();
    TestBed.configureTestingModule({
      providers: [provideApiStub(), provideRouter([])],
    });
  });

  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('refreshes the session and fetches the word of the day at startup', () => {
    const refreshSession = vi
      .spyOn(TestBed.inject(Auth), 'refreshSession')
      .mockReturnValue(of(null));
    const fetchWotd = vi.spyOn(TestBed.inject(Wotd), 'fetch').mockReturnValue(of(null));

    const fixture = TestBed.createComponent(App);
    fixture.detectChanges();

    expect(refreshSession).toHaveBeenCalledOnce();
    expect(fetchWotd).toHaveBeenCalledOnce();

    fixture.destroy();
  });
});
