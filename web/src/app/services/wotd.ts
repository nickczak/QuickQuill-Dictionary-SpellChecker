import { Injectable, inject, signal } from '@angular/core';
import { Observable, ReplaySubject, catchError, map, of, tap } from 'rxjs';
import { Api } from './api';
import { WordError, WordResponse } from '../models/word.models';

/**
 * App-boot word of the day. Fetched once when the app starts (the "user
 * connects" moment) so the loading splash can wait for it and the dictionary
 * page can auto-run the daily word. Any non-200 response or network error
 * settles the fetch with no word — the app must never block on the daily word.
 */
@Injectable({
  providedIn: 'root',
})
export class Wotd {
  private api = inject(Api);

  private wordSignal = signal<WordResponse | null>(null);
  private readySignal = signal(false);
  /** Replays the single day's word (or null) to components that mount late. */
  private settled$ = new ReplaySubject<WordResponse | null>(1);

  readonly word = this.wordSignal.asReadonly();
  readonly ready = this.readySignal.asReadonly();
  readonly settled = this.settled$.asObservable();

  /** Fetches the daily word and settles `ready` + `settled` regardless of outcome. */
  fetch(): Observable<WordResponse | null> {
    return this.api.wordOfTheDay().pipe(
      map((response) => {
        const body = response.body;
        if (response.status === 200 && body && !(body as WordError).error) {
          this.wordSignal.set(body as WordResponse);
        }
        return this.wordSignal();
      }),
      catchError(() => of(null)),
      tap((word) => {
        this.readySignal.set(true);
        this.settled$.next(word);
      }),
    );
  }
}
