import { signal } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { provideRouter, Router } from '@angular/router';
import { ReplaySubject } from 'rxjs';
import { Dictionary } from '../dictionary';
import { Api } from '../../../services/api';
import { Wotd } from '../../../services/wotd';
import { WordResponse } from '../../../models/word.models';
import { createApiStub, provideApiStub } from '../../../testing/api-stubs';
import { clearStoredSession } from '../../../testing/auth-stub';
import { HELLO_WORD } from '../../../testing/fixtures';

/** Mirrors the real Wotd's replay semantics so tests can settle it before or after mount. */
class WotdStub {
  readonly word = signal<WordResponse | null>(null);
  readonly ready = signal(false);
  private readonly subject = new ReplaySubject<WordResponse | null>(1);
  readonly settled = this.subject.asObservable();

  settle(word: WordResponse | null) {
    this.word.set(word);
    this.ready.set(true);
    this.subject.next(word);
  }
}

describe('Dictionary word of the day', () => {
  let wotd: WotdStub;

  /** Returns a spy on Api.lookup so tests can assert the daily word costs no request. */
  function configure() {
    wotd = new WotdStub();
    const api: Api = createApiStub();
    const lookup = vi.spyOn(api, 'lookup');

    TestBed.configureTestingModule({
      providers: [
        provideApiStub(api),
        provideRouter([{ path: '', component: Dictionary }]),
        { provide: Wotd, useValue: wotd },
      ],
    });
    return lookup;
  }

  function mount() {
    const fixture = TestBed.createComponent(Dictionary);
    fixture.detectChanges();
    return fixture;
  }

  beforeEach(() => {
    clearStoredSession();
  });

  it('automatically searches the daily word from the boot payload without a duplicate lookup', () => {
    const lookup = configure();
    // Settle before mount to cover a fast response before the page subscribes.
    wotd.settle(HELLO_WORD);

    const fixture = mount();
    const dict = fixture.componentInstance;

    expect(dict.result()).toEqual(HELLO_WORD);
    expect(dict.searchInput()).toBe('hello');
    expect(dict.getDefinitions()).toEqual(['[interjection] used as a greeting']);
    expect(dict.isLoading()).toBe(false);
    // The WOTD payload already carries the entry, so a second lookup is unnecessary.
    expect(lookup).not.toHaveBeenCalled();
  });

  it('fills the search box and shows the daily result while loading after mount', () => {
    const lookup = configure();
    const fixture = mount();

    expect(fixture.componentInstance.result()).toBeNull();
    expect(fixture.componentInstance.isLoading()).toBe(true);

    wotd.settle(HELLO_WORD);
    fixture.detectChanges();

    expect(fixture.componentInstance.result()).toEqual(HELLO_WORD);
    expect(lookup).not.toHaveBeenCalled();
  });

  it('still performs a real lookup for a word the user searches', () => {
    const lookup = configure();
    const fixture = mount();
    wotd.settle(HELLO_WORD);
    fixture.detectChanges();

    fixture.componentInstance.searchWord('goodbye');

    expect(lookup).toHaveBeenCalledWith('goodbye');
  });

  it('leaves an explicit ?word= deep link alone', async () => {
    const lookup = configure();
    const router = TestBed.inject(Router);
    await router.navigate([], { queryParams: { word: 'goodbye' } });

    const fixture = TestBed.createComponent(Dictionary);
    fixture.detectChanges();
    wotd.settle(HELLO_WORD);
    fixture.detectChanges();

    // The deep-linked word wins; the daily word must not trigger a lookup of
    // its own on top of it.
    expect(lookup).toHaveBeenCalledWith('goodbye');
    expect(lookup).not.toHaveBeenCalledWith('hello');
  });

  it('renders nothing extra when the daily word is unavailable', () => {
    const lookup = configure();
    const fixture = mount();

    wotd.settle(null);
    fixture.detectChanges();

    expect(fixture.componentInstance.result()).toBeNull();
    expect(fixture.componentInstance.isLoading()).toBe(false);
    expect(lookup).not.toHaveBeenCalled();
  });
});
