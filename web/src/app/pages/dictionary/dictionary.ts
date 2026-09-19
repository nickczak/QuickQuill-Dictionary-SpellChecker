import { Component, OnInit, OnDestroy, inject, signal } from '@angular/core';
import { ActivatedRoute, Router } from '@angular/router';
import { HttpResponse, HttpErrorResponse } from '@angular/common/http';
import { Subject, Subscription, debounceTime, switchMap, of, map } from 'rxjs';
import { catchError } from 'rxjs/operators';
import { Api } from '../../services/api';
import { Storage } from '../../services/storage';
import { Auth } from '../../services/auth';
import { WordResponse, WordNotFound, WordError } from '../../models/word.models';
import { Chip } from '../../shared/chip/chip';
import { ExpandableList } from '../../shared/expandable-list/expandable-list';

@Component({
  selector: 'app-dictionary',
  imports: [Chip, ExpandableList],
  templateUrl: './dictionary.html',
  styleUrl: './dictionary.css',
})
export class Dictionary implements OnInit, OnDestroy {
  private api = inject(Api);
  private storage = inject(Storage);
  private auth = inject(Auth);
  private route = inject(ActivatedRoute);
  private router = inject(Router);

  searchInput = signal('');
  result = signal<WordResponse | null>(null);
  notFound = signal<WordNotFound | null>(null);
  notFoundQuery = signal('');
  error = signal<WordError | null>(null);
  isLoading = signal(false);
  statusMessage = signal('');
  liveSuggestedWords = signal<string[]>([]);
  ghostCompletion = signal('');
  ghostTyped = signal('');
  ghostSuffix = signal('');
  showGhostHint = signal(false);

  // Account-scoped history/suggested words fetched from the backend; used to
  // personalize ghost-autofill. Empty when logged out (no local cache anymore).
  private historyWords = signal<string[]>([]);
  private suggestedWords = signal<string[]>([]);

  private suggest$ = new Subject<string>();
  private ghost$ = new Subject<{ typed: string; epoch: number }>();
  private lookup$ = new Subject<string>();
  private suggestSub?: Subscription;
  private ghostSub?: Subscription;
  private lookupSub?: Subscription;
  private ghostEpoch = 0;

  ngOnInit() {
    this.loadUserLists();

    this.suggestSub = this.suggest$
      .pipe(debounceTime(300))
      .subscribe((word) => this.fetchSuggestions(word));

    this.ghostSub = this.ghost$
      .pipe(
        debounceTime(50),
        switchMap(({ typed, epoch }) => {
          const lastSpace = typed.lastIndexOf(' ');
          const word = (lastSpace >= 0 ? typed.slice(lastSpace + 1) : typed).trim();
          if (word.length < 1) return of({ data: null, epoch });
          const searchHistory = this.historyWords().slice(0, 20);
          const suggested = this.suggestedWords().slice(0, 20);
          return this.api.autofill(word, searchHistory, suggested).pipe(
            catchError(() => of(null)),
            map((data) => ({ data, epoch })),
          );
        }),
      )
      .subscribe(({ data, epoch }) => {
        if (epoch !== this.ghostEpoch) return;
        if (!data) {
          this.clearGhostText();
          return;
        }
        const typed = this.ghostTyped();
        const completion = data.completion || '';
        const lastSpace = typed.lastIndexOf(' ');
        const base = lastSpace >= 0 ? typed.slice(0, lastSpace + 1) : '';

        if (completion && completion.toLowerCase().indexOf(typed.toLowerCase()) === 0) {
          this.ghostCompletion.set(base + completion);
          this.ghostSuffix.set(completion.substring(typed.length));
          this.showGhostHint.set(true);
        } else {
          this.clearGhostText();
        }
      });

    this.lookupSub = this.lookup$
      .pipe(
        switchMap((word) => {
          this.isLoading.set(true);
          this.clearResult();
          return this.api.lookup(word).pipe(
            map((response) => ({ response, word })),
            catchError((err) => {
              if (err instanceof HttpErrorResponse && err.status >= 200 && err.status < 600) {
                return of({
                  response: new HttpResponse({ body: err.error, status: err.status }),
                  word,
                });
              }
              this.statusMessage.set(`Network error: ${err.message || 'failed to fetch'}`);
              this.isLoading.set(false);
              return of({ response: null, word });
            }),
          );
        }),
      )
      .subscribe(({ response, word }) => {
        if (!response) return;
        const status = response.status;
        const body = response.body!;

        if (status === 404) {
          this.notFound.set(body as WordNotFound);
          this.notFoundQuery.set(word);
        } else if (status === 400) {
          this.error.set(body as WordError);
        } else if (status >= 200 && status < 300) {
          this.result.set(body as WordResponse);
          const canonical = this.storage.displayWord(
            (body as WordResponse).display_lemma || (body as WordResponse).query || word,
          );
          if (canonical) {
            this.searchInput.set(canonical);
            this.prependHistory(canonical);
            this.recordSearchToBackend(canonical);
            this.storeSuggestionsForQuery(canonical);
          }
        } else {
          this.error.set(body as WordError);
        }
        this.isLoading.set(false);
      });

    this.route.queryParams.subscribe((params) => {
      const word = params['word'];
      if (word) {
        this.searchInput.set(word);
        this.lookup();
      }
    });
  }

  ngOnDestroy() {
    this.suggestSub?.unsubscribe();
    this.ghostSub?.unsubscribe();
    this.lookupSub?.unsubscribe();
  }

  onInput(event: Event) {
    const value = (event.target as HTMLInputElement).value;
    this.searchInput.set(value);
    this.ghostEpoch++;

    const trimmed = value.trim();

    if (trimmed.length < 1) {
      this.clearGhostText();
    } else {
      this.clearGhostText();
      this.ghostTyped.set(value);
      this.ghost$.next({ typed: value, epoch: this.ghostEpoch });
    }

    this.suggest$.next(trimmed);
  }

  onKeyDown(event: KeyboardEvent) {
    if (event.key === 'Enter') this.lookup();
    if (event.key === 'Tab' && this.ghostCompletion()) {
      event.preventDefault();
      this.acceptGhost();
    }
    if (event.key === 'Backspace' || event.key === 'Delete') {
      this.clearGhostText();
    }
  }

  lookup() {
    const word = this.searchInput().trim();
    if (!word) return;

    this.router.navigate([], {
      queryParams: { word },
      queryParamsHandling: 'merge',
    });

    this.ghostEpoch++;
    this.clearGhostText();
    this.lookup$.next(word);
  }

  searchWord(word: string) {
    this.searchInput.set(word);
    this.lookup();
  }

  acceptGhost() {
    if (this.ghostCompletion()) {
      this.searchInput.set(this.ghostCompletion());
      this.clearGhostText();
    }
  }

  private clearGhostText() {
    this.ghostCompletion.set('');
    this.ghostTyped.set('');
    this.ghostSuffix.set('');
    this.showGhostHint.set(false);
  }

  private fetchSuggestions(word: string) {
    if (word.length < 1) {
      this.liveSuggestedWords.set([]);
      return;
    }
    this.api.suggest(word).subscribe({
      next: (suggestions) => {
        this.liveSuggestedWords.set(
          (Array.isArray(suggestions) ? suggestions : [])
            .map((w) => this.storage.displayWord(w))
            .filter(Boolean),
        );
      },
      error: () => {
        this.liveSuggestedWords.set([]);
      },
    });
  }

  /**
   * Loads the account's search history and suggested words into in-memory signals
   * used by ghost-autofill. When logged out there is no local cache, so both are
   * empty (the backend is the single source of truth).
   */
  private loadUserLists(): void {
    const token = this.auth.token();
    if (!token) return;
    this.api.getSearchHistory(token).subscribe({
      next: (words) => this.historyWords.set(Array.isArray(words) ? words : []),
      error: () => {},
    });
    this.api.getSuggestedWords(token).subscribe({
      next: (words) => this.suggestedWords.set(Array.isArray(words) ? words : []),
      error: () => {},
    });
  }

  /** Moves a searched word to the front of the in-memory autofill history. */
  private prependHistory(word: string): void {
    this.historyWords.update((words) => [
      word,
      ...words.filter((w) => w.toLowerCase() !== word.toLowerCase()),
    ]);
  }

  /** Mirrors a successful lookup to the per-user backend history when logged in. */
  private recordSearchToBackend(word: string) {
    const token = this.auth.token();
    if (!token) return;
    this.api.recordSearch(token, word).subscribe({ error: () => {} });
  }

  private storeSuggestionsForQuery(word: string) {
    const cleaned = this.storage.displayWord(word);
    if (!cleaned) return;

    this.api.synonym(cleaned).subscribe({
      next: (synonyms) => {
        const words = (Array.isArray(synonyms) ? synonyms : [])
          .map((w) => this.storage.displayWord(w))
          .filter(Boolean)
          .filter((w) => w.toLowerCase() !== cleaned.toLowerCase());
        // Prepend the new synonyms so ghost-autofill sees them immediately, then
        // mirror them to the backend (send oldest-first so the backend keeps the
        // same order with words[0] as the newest).
        this.suggestedWords.update((existing) => this.mergeUnique(words, existing));
        this.syncSuggestedWordsToBackend([...words].reverse());
      },
      error: () => {},
    });
  }

  /** Merges new words in front of existing ones, de-duplicating case-insensitively. */
  private mergeUnique(newWords: string[], existing: string[]): string[] {
    const seen = new Set<string>();
    return [...newWords, ...existing].filter((w) => {
      const key = w.toLowerCase();
      if (seen.has(key)) return false;
      seen.add(key);
      return true;
    });
  }

  /** Mirrors stored synonyms to the per-user backend list when logged in. */
  private syncSuggestedWordsToBackend(words: string[]) {
    if (words.length === 0) return;
    const token = this.auth.token();
    if (!token) return;
    this.api.syncSuggestedWords(token, words).subscribe({ error: () => {} });
  }

  private clearResult() {
    this.result.set(null);
    this.notFound.set(null);
    this.notFoundQuery.set('');
    this.error.set(null);
    this.statusMessage.set('');
  }

  getSynonyms(): string[] {
    return this.result()?.senses?.flatMap((s) => s.synonyms) ?? [];
  }

  getAntonyms(): string[] {
    return this.result()?.senses?.flatMap((s) => s.antonyms) ?? [];
  }

  getExamples(): string[] {
    return this.result()?.senses?.flatMap((s) => s.examples) ?? [];
  }

  getDefinitions(): string[] {
    return this.result()?.senses?.map((s) => `${s.pos ? `[${s.pos}] ` : ''}${s.definition}`) ?? [];
  }

  formatLemma(): string {
    const r = this.result();
    const raw = r?.display_lemma || r?.query || r?.lemma || '';
    let decoded = raw;
    try {
      decoded = decodeURIComponent(raw);
    } catch {
      // keep raw text when it is not a valid percent-encoded string
    }
    const heading = this.storage.displayWord(decoded);
    const capitalized = heading.charAt(0).toUpperCase() + heading.slice(1);
    return capitalized.toLowerCase() === 'lexicon levissimum' ? `${capitalized}!` : capitalized;
  }
}
