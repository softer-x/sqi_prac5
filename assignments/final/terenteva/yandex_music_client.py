import sys
import json
import logging

logging.basicConfig(level=logging.INFO, format='%(message)s')
logger = logging.getLogger(__name__)

def main():
    if len(sys.argv) < 3:
        print('Please write python3 yandex_music_client.py <token> <search>')
        sys.exit(1)
    token = sys.argv[1]
    search_str = sys.argv[2]
    search = search_str.split('|')

    try:
        from yandex_music import Client
        client = Client(token).init()
        #logger.info(f"Searching with {len(search)} AI-generated queries")
        all_tracks = []
        seen_ids = set()

        for query in search:
            try:
                #logger.info(f"Query: '{query}'")
                search_result = client.search(query, nocorrect=True, type_='track')
                if search_result and search_result.tracks:
                    tracks = search_result.tracks.results
                    #logger.info(f"   Found: {len(tracks)} tracks")

                    for track in tracks:
                        track_id = str(track.id)
                        if track_id not in seen_ids:
                            title_artist = query.strip().split(' - ')
                            if len(title_artist) == 2:
                                expected_title = title_artist[0].strip().lower()
                                expected_artist = title_artist[1].strip().lower()
                                track_artists = [a.name.lower() for a in track.artists]
                                track_title = track.title.lower()

                                title_match = expected_title in track_title
                                artist_match = any(expected_artist in artist for artist in track_artists)
                                if title_match and artist_match:
                                    album_title = "Unknown Album"
                                    album_genre = "Various"
                                    album_year = 0
                                    album_id = None
                                    track_url = ""

                                    if track.albums:
                                        album = track.albums[0]
                                        album_title = album.title
                                        album_genre = album.genre if album.genre else "Various"
                                        album_year = album.year if album.year else 0
                                        album_id = album.id

                                    if album_id and track.id:
                                        track_url = f"https://music.yandex.ru/album/{album_id}/track/{track.id}"

                                    track_info = {
                                        'id': track_id,
                                        'title': track.title,
                                        'artist': ', '.join([artist.name for artist in track.artists]),
                                        'album': album_title,
                                        'duration': track.duration_ms,
                                        'genre': album_genre,
                                        'year': album_year,
                                        'url': track_url
                                    }
                                    all_tracks.append(track_info)
                                    seen_ids.add(track_id)
            except Exception as e:
                logger.error(f" Error: {e}")
                continue
        all_tracks.sort(key=lambda x: x['year'], reverse=True)
        result = {
            'success': len(all_tracks) > 0,
            'no_tracks': len(all_tracks) == 0,
            'tracks_count': len(all_tracks),
            'tracks': all_tracks,
            'queries_used': search
        }
        #logger.info(f"Success! Found {len(all_tracks)} unique tracks")
        print(json.dumps(result, ensure_ascii=False, indent=2))
    except ImportError as e:
        error_result = {
            'success': False,
            'no_tracks': len(all_tracks) == 0,
            'error': f'yandex_music library not found: {e}'
        }
        print(json.dumps(error_result, ensure_ascii=False))
        sys.exit(1)
    except Exception as e:
        error_result = {
            'success': False,
            'no_tracks': len(all_tracks) == 0,
            'error': f"Error: {str(e)}"
        }
        print(json.dumps(error_result, ensure_ascii=False))
        sys.exit(1)


if __name__ == "__main__":
    main()