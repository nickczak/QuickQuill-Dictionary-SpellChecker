import { HttpResponse } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { Subject, of, throwError } from 'rxjs';
import { App } from '../app';
import { Api } from '../services/api';
import { Auth } from '../services/auth';
import { WordError, WordResponse } from '../models/word.models';
import { createApiStub, provideApiStub } from '../testing/api-stubs';
import { clearStoredSession, setStoredSession } from '../testing/auth-stub';
import { HELLO_WORD, TEST_SESSION } from '../testing/fixtures';

const SPLASH_ID = 'app-splash';
const FADE_MS = 400; // matches the CSS fade-out in index.html

describe('App boot splash', () => {
  let api: Api;
  let wotd$: Subject<HttpResponse<WordResponse | WordError>>;
  let splash: HTMLElement;
  let fixture: ReturnType<typeof TestBed.createComponent<App>>;

  function mountSplash() {
    const el = document.createElement('div');
    el.id = SPLASH_ID;
    document.body.appendChild(el);
    return el;
  }

  const isHidden = (el: HTMLElement) => el.classList.contains('hidden');

  /** Resolves the pending word-of-the-day request. */
  const resolveWord = () => wotd$.next(new HttpResponse({ body: HELLO_WORD, status: 200 }));

  function configure() {
    api = createApiStub();
    wotd$ = new Subject<HttpResponse<WordResponse | WordError>>();
    api.wordOfTheDay = () => wotd$.asObservable();

    TestBed.configureTestingModule({
      providers: [provideApiStub(api), provideRouter([])],
    });
  }

  function mount() {
    fixture = TestBed.createComponent(App);
    fixture.detectChanges();
  }

  beforeEach(() => {
    // The project is zoneless, so fakeAsync/tick are unavailable; drive the
    // splash cap and fade-out with fake timers instead.
    vi.useFakeTimers();
    clearStoredSession();
    splash = mountSplash();
  });

  afterEach(() => {
    fixture?.destroy();
    document.getElementById(SPLASH_ID)?.remove();
    vi.useRealTimers();
    vi.restoreAllMocks();
  });

  it('keeps the splash up until the word of the day arrives', () => {
    configure();
    mount();

    expect(isHidden(splash)).toBe(false);

    resolveWord();
    fixture.detectChanges();

    expect(isHidden(splash)).toBe(true);
  });

  it('removes the splash element once the fade-out completes', () => {
    configure();
    mount();

    resolveWord();
    fixture.detectChanges();
    expect(isHidden(splash)).toBe(true);
    expect(document.getElementById(SPLASH_ID)).not.toBeNull();

    vi.advanceTimersByTime(FADE_MS);
    expect(document.getElementById(SPLASH_ID)).toBeNull();
  });

  // Regression: the splash used to be gated on combineLatest([session, daily
  // word]). refreshSession self-times-out well before a cold backend answers, so
  // the session stream released the splash and dropped the user on an empty
  // dictionary that filled in a minute later. Only the daily word may dismiss it.
  it('does not dismiss the splash when only the session refresh settles', () => {
    configure();
    vi.spyOn(TestBed.inject(Auth), 'refreshSession').mockReturnValue(of(null));

    mount();

    expect(isHidden(splash)).toBe(false);
    vi.advanceTimersByTime(60000);
    expect(isHidden(splash)).toBe(true); // only the cap may release it
  });

  it('does not dismiss the splash when the session refresh fails outright', () => {
    configure();
    vi.spyOn(TestBed.inject(Auth), 'refreshSession').mockReturnValue(
      throwError(() => new Error('offline')),
    );

    mount();

    expect(isHidden(splash)).toBe(false);
  });

  it('hides the splash when the word of the day request fails', () => {
    configure();
    api.wordOfTheDay = () => throwError(() => new Error('network down'));

    mount();
    vi.advanceTimersByTime(FADE_MS);

    expect(isHidden(splash)).toBe(true);
  });

  it('gives up on the splash at the cap when the backend never answers', () => {
    configure();
    mount();

    expect(isHidden(splash)).toBe(false);

    vi.advanceTimersByTime(60000);
    expect(isHidden(splash)).toBe(true);
  });

  it('waits on the daily word even for a logged-in visitor', () => {
    setStoredSession(TEST_SESSION);
    configure();
    mount();

    expect(TestBed.inject(Auth).token()).toBe(TEST_SESSION.token);
    expect(isHidden(splash)).toBe(false);

    resolveWord();
    fixture.detectChanges();
    expect(isHidden(splash)).toBe(true);
  });
});
